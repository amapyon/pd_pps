/*
 * M7 USB CDC-ACM ASCII commands experiment for CH32X035 / ch32fun.
 * Copyright (c) 2026. Released under the MIT license (see LICENSE).
 *
 * EP0: control, EP1 IN: CDC notification, EP2 OUT: host->device,
 * EP3 IN: device->host. The descriptor comes from the pinned ch32fun
 * USBFS CDC TTY example, copied into this experiment as usb_config.h.
 *
 * No USB-PD negotiation, output control or unconfigured sensor readings.
 * Measurement collection is abstracted in m7_measurement.c.
 */
#include <stdint.h>
#include <stddef.h>
#include "m7_measurement.h"

#ifdef M7_HOST_TEST
#include "host_test/mock_usb.h"
#else
#include "ch32fun.h"
#include "fsusb.h"
#endif

#ifndef CDC_SET_LINE_CODING
#define CDC_SET_LINE_CODING 0x20u
#endif
#ifndef CDC_GET_LINE_CODING
#define CDC_GET_LINE_CODING 0x21u
#endif
#define CDC_SET_CONTROL_LINE_STATE  0x22u
#ifndef CDC_SEND_BREAK
#define CDC_SEND_BREAK 0x23u
#endif
/* CDC ACM class/interface requests: OUT=0x21, IN=0xA1. */
#define CDC_CLASS_INTERFACE_OUT     0x21u
#define CDC_CLASS_INTERFACE_IN      0xA1u

#define CDC_OUT_EP 2
#define CDC_IN_EP  3
#define RX_RING_SIZE 256u
/* EP2 OUT accepts full-speed bulk packets of up to 64 bytes. */
#define CDC_OUT_MAX_PACKET_SIZE 64u
/* EP3 IN intentionally uses short packets (<64 bytes) to terminate each
 * transfer without a zero-length packet (ZLP). This is conservative and
 * suitable for the M7 command experiment; optimize later if needed.
 */
#define CDC_TX_CHUNK_SIZE 63u

/* uint8_t indices wrap naturally at 256; capacity is 255 bytes. */
static volatile uint8_t rx_ring[RX_RING_SIZE];
static volatile uint8_t rx_write_idx;
static volatile uint8_t rx_read_idx;
static volatile uint32_t rx_overflows;

/* 115200 8N1 is a CDC line coding convention, not a physical UART baud. */
static uint8_t cdc_line_coding[7] = {
    0x00, 0xC2, 0x01, 0x00, /* dwDTERate: 115200 */
    0x00,                   /* one stop bit */
    0x00,                   /* no parity */
    0x08                    /* 8 data bits */
};
static volatile uint8_t expecting_line_coding;

/*
 * M7 USBFS diagnostic snapshot (version 1): RAM-only, no CDC output.
 * All fields are 32-bit words for straightforward SRAM/ELF decoding.
 * ISR counters and foreground counters have independent writers.
 * Snapshot register fields are sampled by the MCU itself, NOT by WCH-LinkE.
 */
typedef struct {
    uint32_t magic;               /* 0x4D364447 = M7DG */
    uint32_t version;             /* 1 */
    uint32_t rx_ep2_packets;
    uint32_t rx_ep2_bytes;
    uint32_t ep3_in_completed;    /* called by fsusb IRQ for EP3 IN */
    uint32_t tx_attempts;
    uint32_t tx_accepted;
    uint32_t tx_rejected;
    uint32_t tx_bytes_accepted;
    uint32_t tx_last_len;
    int32_t  tx_last_rc;
    uint32_t tx_reject_minus1;
    uint32_t tx_reject_minus2;
    uint32_t tx_reject_minus3;
    uint32_t ep3_busy_polls;
    uint32_t ep3_busy_snapshots;
    uint32_t busy_hw_base_ctrl;
    uint32_t busy_hw_udev_ctrl;
    uint32_t busy_hw_int_en;
    uint32_t busy_hw_int_fg;
    uint32_t busy_hw_int_st;
    uint32_t busy_hw_uep2_3_mod;
    uint32_t busy_hw_ep3_tx_len;
    uint32_t busy_hw_ep3_tx_ctrl;
    uint32_t busy_ctx_ep3_busy;
    uint32_t busy_ctx_ep3_mode;
    uint32_t busy_ctx_setup_len;
    uint32_t busy_ctx_errata_window;
} M7_UsbDiagnostics;

/* 'used' ensures the object survives link-time optimization for RAM capture. */
volatile M7_UsbDiagnostics m7_usb_diag __attribute__((used));

static void M7_DiagSampleBusy(void)
{
    /* Rate limit MMIO sampling in the tight foreground poll loop. */
    m7_usb_diag.ep3_busy_polls++;
    if ((m7_usb_diag.ep3_busy_polls & 0x3fffu) != 0u) return;
    m7_usb_diag.ep3_busy_snapshots++;
#ifndef M7_HOST_TEST
    m7_usb_diag.busy_hw_base_ctrl = USBFS->BASE_CTRL;
    m7_usb_diag.busy_hw_udev_ctrl = USBFS->UDEV_CTRL;
    m7_usb_diag.busy_hw_int_en = USBFS->INT_EN;
    m7_usb_diag.busy_hw_int_fg = USBFS->INT_FG;
    m7_usb_diag.busy_hw_int_st = USBFS->INT_ST;
    m7_usb_diag.busy_hw_uep2_3_mod = USBFS->UEP2_3_MOD;
    m7_usb_diag.busy_hw_ep3_tx_len = UEP_CTRL_LEN(CDC_IN_EP);
    m7_usb_diag.busy_hw_ep3_tx_ctrl = UEP_CTRL_TX(CDC_IN_EP);
    m7_usb_diag.busy_ctx_ep3_busy = USBFSCTX.endpoints[CDC_IN_EP].busy;
    m7_usb_diag.busy_ctx_ep3_mode = USBFSCTX.endpoints[CDC_IN_EP].mode;
    m7_usb_diag.busy_ctx_setup_len = USBFSCTX.USBFS_SetupReqLen;
    m7_usb_diag.busy_ctx_errata_window = USBFSCTX.USBFS_errata_dont_send_endpoint_in_window;
#endif
}

static void rx_push(uint8_t byte)
{
    uint8_t next = (uint8_t)(rx_write_idx + 1u);
    if (next == rx_read_idx) {
        rx_overflows++;
        return; /* no overwrite of unread bytes */
    }
    rx_ring[rx_write_idx] = byte;
    rx_write_idx = next;
}

/* Called from the USBFS interrupt handler for an OUT packet. */
void HandleDataOut(struct _USBState *ctx, int endpoint, uint8_t *data, int len)
{
    if (endpoint == 0) {
        if (expecting_line_coding && len == (int)sizeof(cdc_line_coding)) {
            for (unsigned i = 0; i < sizeof(cdc_line_coding); ++i) {
                cdc_line_coding[i] = data[i];
            }
        }
        expecting_line_coding = 0;
        ctx->USBFS_SetupReqLen = 0;
        return;
    }
    if (endpoint == CDC_OUT_EP && len > 0 && len <= (int)CDC_OUT_MAX_PACKET_SIZE) {
        m7_usb_diag.rx_ep2_packets++;
        m7_usb_diag.rx_ep2_bytes += (uint32_t)len;
        for (int i = 0; i < len; ++i) rx_push(data[i]);
    }
}

/* IN completion callback: fsusb.c clears EP3 busy before calling this. */
int HandleInRequest(struct _USBState *ctx, int endpoint, uint8_t *data, int len)
{
    (void)ctx;
    if (endpoint == CDC_IN_EP) m7_usb_diag.ep3_in_completed++;
    (void)data;
    (void)len;
    return 0;
}

/*
 * fsusb.c (the version bundled with this project) interprets the custom
 * control handler's return value as follows:
 *   > 0: data-stage length
 *   = 0: STALL (request not supported)
 *   < 0: ACK a request with no data stage (zero-length status IN packet)
 * Do NOT return 0 for a valid CDC SET_CONTROL_LINE_STATE: Windows may fail
 * to open the virtual COM port when that request is STALLed.
 */
int HandleSetupCustom(struct _USBState *ctx, int setup_code)
{
    /* Prevent an earlier GET_LINE_CODING pointer leaking into the next SETUP. */
    ctx->pCtrlPayloadPtr = NULL;
    expecting_line_coding = 0;

    switch ((unsigned)setup_code) {
    case CDC_SET_LINE_CODING:
        if (ctx->USBFS_SetupReqType != CDC_CLASS_INTERFACE_OUT ||
            ctx->USBFS_SetupReqLen != sizeof(cdc_line_coding)) return 0;
        expecting_line_coding = 1;
        return (int)sizeof(cdc_line_coding);

    case CDC_GET_LINE_CODING:
        if (ctx->USBFS_SetupReqType != CDC_CLASS_INTERFACE_IN ||
            ctx->USBFS_SetupReqLen == 0) return 0;
        ctx->pCtrlPayloadPtr = cdc_line_coding;
        return (ctx->USBFS_SetupReqLen < sizeof(cdc_line_coding)) ?
            (int)ctx->USBFS_SetupReqLen : (int)sizeof(cdc_line_coding);

    case CDC_SET_CONTROL_LINE_STATE:
    case CDC_SEND_BREAK:
        if (ctx->USBFS_SetupReqType != CDC_CLASS_INTERFACE_OUT ||
            ctx->USBFS_SetupReqLen != 0) return 0;
        return -1;  /* fsusb.c sends a zero-length status ACK */

    default:
        return 0;   /* Unsupported request: STALL */
    }
}

/*
 * Foreground-only line protocol. IRQ writes rx_ring; main drains it.
 * One response at a time, no heap, no C stdio and no PD control.
 * Frames are ASCII commands ending in LF (CRLF also accepted).
 */
#define M7_MAX_COMMAND_LENGTH 31u
#define M7_REPLY_CAPACITY 192u

static uint8_t command[M7_MAX_COMMAND_LENGTH + 1u];
static uint8_t command_length;
static uint8_t command_too_long;
static uint8_t command_invalid;
static uint32_t command_errors;

static uint8_t reply[M7_REPLY_CAPACITY];
static uint16_t reply_length;
static uint16_t reply_sent;

static void M7_AppendChar(char c)
{
    /* Reserve space for the terminal newline. */
    if (reply_length < M7_REPLY_CAPACITY - 1u)
        reply[reply_length++] = (uint8_t)c;
}
static void M7_AppendText(const char *p)
{
    while (*p) M7_AppendChar(*p++);
}
static void M7_AppendU32(uint32_t value)
{
    char digits[10];
    uint8_t n = 0;
    do {
        digits[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value && n < sizeof(digits));
    while (n) M7_AppendChar(digits[--n]);
}
/* Safe for INT32_MIN: unsigned subtraction avoids signed negation overflow. */
static void M7_AppendI32(int32_t value)
{
    if (value < 0) {
        M7_AppendChar('-');
        M7_AppendU32(0u - (uint32_t)value);
    } else {
        M7_AppendU32((uint32_t)value);
    }
}
static void M7_FinishReply(void)
{
    reply[reply_length++] = (uint8_t)'\n';
    reply_sent = 0;
}
static int M7_CommandIs(const char *candidate)
{
    uint8_t i = 0;
    while (candidate[i] && i < command_length) {
        if ((uint8_t)candidate[i] != command[i]) return 0;
        ++i;
    }
    return i == command_length && candidate[i] == '\0';
}
static void M7_BuildResponse(void)
{
    reply_length = 0;
    reply_sent = 0;
    if (command_too_long) {
        ++command_errors;
        M7_AppendText("ERR line_too_long");
    } else if (command_invalid) {
        ++command_errors;
        M7_AppendText("ERR invalid_character");
    } else if (M7_CommandIs("INFO")) {
        M7_AppendText("INFO board=CH32X035F8U6 firmware=m7_usb_cdc_measure protocol=2");
    } else if (M7_CommandIs("STATUS")) {
        M7_AppendText("STATUS rx_packets=");
        M7_AppendU32(m7_usb_diag.rx_ep2_packets);
        M7_AppendText(" rx_bytes=");
        M7_AppendU32(m7_usb_diag.rx_ep2_bytes);
        M7_AppendText(" tx_packets=");
        M7_AppendU32(m7_usb_diag.tx_accepted);
        M7_AppendText(" tx_bytes=");
        M7_AppendU32(m7_usb_diag.tx_bytes_accepted);
        M7_AppendText(" rx_overflows=");
        M7_AppendU32(rx_overflows);
        M7_AppendText(" errors=");
        M7_AppendU32(command_errors);
    } else if (M7_CommandIs("MEASURE")) {
        M7_Measurement sample;
        /* Only the measurement backend may claim validity of real values. */
        M7_ReadMeasurement(&sample);
        if (sample.status == M7_MEASURE_OK) {
            M7_AppendText("MEASURE status=ok voltage_mV=");
            M7_AppendU32(sample.voltage_mV);
            M7_AppendText(" current_mA=");
            M7_AppendI32(sample.current_mA);
        } else if (sample.status == M7_MEASURE_NOT_CONFIGURED) {
            M7_AppendText("MEASURE status=unavailable reason=not_configured voltage_mV=NA current_mA=NA");
        } else if (sample.status == M7_MEASURE_IO_ERROR) {
            M7_AppendText("MEASURE status=error reason=sensor_io voltage_mV=NA current_mA=NA");
        } else {
            M7_AppendText("MEASURE status=error reason=backend_invalid voltage_mV=NA current_mA=NA");
        }
    } else if (M7_CommandIs("PING")) {
        M7_AppendText("PONG");
    } else if (M7_CommandIs("HELP")) {
        M7_AppendText("HELP INFO STATUS MEASURE PING HELP");
    } else {
        ++command_errors;
        M7_AppendText("ERR unknown_command");
    }
    M7_FinishReply();
    command_length = 0;
    command_too_long = 0;
    command_invalid = 0;
}
static void M7_ConsumeByte(uint8_t ch)
{
    if (ch == (uint8_t)'\r') return;
    if (ch == (uint8_t)'\n') {
        M7_BuildResponse();
        return;
    }
    if (ch < 0x20u || ch > 0x7Eu) command_invalid = 1;
    if (command_length < M7_MAX_COMMAND_LENGTH)
        command[command_length++] = ch;
    else
        command_too_long = 1;
}

/* Called repeatedly from main. At most one USB IN submit or one RX byte
 * is processed per iteration; failed submits do not discard response data.
 */
void M7_ServiceUsbCommands(void)
{
    if (reply_sent < reply_length) {
        uint8_t *ep_buffer = USBFS_GetEPBufferIfAvailable(CDC_IN_EP);
        if (ep_buffer == NULL) {
            M7_DiagSampleBusy();
            return;
        }
        uint16_t remain = (uint16_t)(reply_length - reply_sent);
        uint8_t len = (uint8_t)(remain > CDC_TX_CHUNK_SIZE ? CDC_TX_CHUNK_SIZE : remain);
        for (uint8_t i = 0; i < len; ++i)
            ep_buffer[i] = reply[reply_sent + i];
        m7_usb_diag.tx_attempts++;
        m7_usb_diag.tx_last_len = len;
        int rc = USBFS_SendEndpoint(CDC_IN_EP, len);
        m7_usb_diag.tx_last_rc = rc;
        if (rc == 0) {
            m7_usb_diag.tx_accepted++;
            m7_usb_diag.tx_bytes_accepted += len;
            reply_sent = (uint16_t)(reply_sent + len);
        } else {
            m7_usb_diag.tx_rejected++;
            if (rc == -1) m7_usb_diag.tx_reject_minus1++;
            if (rc == -2) m7_usb_diag.tx_reject_minus2++;
            if (rc == -3) m7_usb_diag.tx_reject_minus3++;
        }
        return;
    }
    if (rx_read_idx != rx_write_idx) {
        uint8_t ch = rx_ring[rx_read_idx];
        rx_read_idx = (uint8_t)(rx_read_idx + 1u);
        M7_ConsumeByte(ch);
    }
}

#ifndef M7_HOST_TEST
int main(void)
{
    SystemInit();
    m7_usb_diag.magic = 0x4d374447u; /* M7DG */
    m7_usb_diag.version = 1u;
    USBFSSetup();
    for (;;) M7_ServiceUsbCommands();
}
#endif
