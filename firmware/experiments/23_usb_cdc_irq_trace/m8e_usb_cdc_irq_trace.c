/*
 * M8D USB EP3 diagnostic experiment (cloned from M8) for CH32X035 / ch32fun.
 * Copyright (c) 2026. Released under the MIT license (see LICENSE).
 *
 * EP0: control, EP1 IN: CDC notification, EP2 OUT: host->device,
 * EP3 IN: device->host. The descriptor comes from the pinned ch32fun
 * USBFS CDC TTY example, copied into this experiment as usb_config.h.
 *
 * No USB-PD negotiation or output control. Trace is SRAM-only. INA226 values require successful I2C reads.
 * Sensor collection is abstracted in m8_measurement.c/m8_ina226.c.
 */
#include <stdint.h>
#include <stddef.h>
#include "m8_measurement.h"

#ifdef M8_HOST_TEST
#include "host_test/mock_usb.h"
#else
#include "ch32fun.h"
#include "fsusb.h"
#include "m8d_trace.h"
#include "m8e_irq_trace.h"
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
 * suitable for the M8 command experiment; optimize later if needed.
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
 * M8 USBFS diagnostic snapshot (version 1): RAM-only, no CDC output.
 * All fields are 32-bit words for straightforward SRAM/ELF decoding.
 * ISR counters and foreground counters have independent writers.
 * Snapshot register fields are sampled by the MCU itself, NOT by WCH-LinkE.
 */
typedef struct {
    uint32_t magic;               /* 0x4D384447 = M8DG */
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
} M8_UsbDiagnostics;

/* 'used' ensures the object survives link-time optimization for RAM capture. */
volatile M8_UsbDiagnostics m8_usb_diag __attribute__((used));

static void M8_DiagSampleBusy(void)
{
    /* Rate limit MMIO sampling in the tight foreground poll loop. */
    m8_usb_diag.ep3_busy_polls++;
    if ((m8_usb_diag.ep3_busy_polls & 0x3fffu) != 0u) return;
    m8_usb_diag.ep3_busy_snapshots++;
#ifndef M8_HOST_TEST
    m8_usb_diag.busy_hw_base_ctrl = USBFS->BASE_CTRL;
    m8_usb_diag.busy_hw_udev_ctrl = USBFS->UDEV_CTRL;
    m8_usb_diag.busy_hw_int_en = USBFS->INT_EN;
    m8_usb_diag.busy_hw_int_fg = USBFS->INT_FG;
    m8_usb_diag.busy_hw_int_st = USBFS->INT_ST;
    m8_usb_diag.busy_hw_uep2_3_mod = USBFS->UEP2_3_MOD;
    m8_usb_diag.busy_hw_ep3_tx_len = UEP_CTRL_LEN(CDC_IN_EP);
    m8_usb_diag.busy_hw_ep3_tx_ctrl = UEP_CTRL_TX(CDC_IN_EP);
    m8_usb_diag.busy_ctx_ep3_busy = USBFSCTX.endpoints[CDC_IN_EP].busy;
    m8_usb_diag.busy_ctx_ep3_mode = USBFSCTX.endpoints[CDC_IN_EP].mode;
    m8_usb_diag.busy_ctx_setup_len = USBFSCTX.USBFS_SetupReqLen;
    m8_usb_diag.busy_ctx_errata_window = USBFSCTX.USBFS_errata_dont_send_endpoint_in_window;
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
        m8_usb_diag.rx_ep2_packets++;
#ifndef M8_HOST_TEST
        M8D_TraceIRQ(M8D_EV_RX_EP2, (uint32_t)len,
            m8_usb_diag.tx_accepted, m8_usb_diag.ep3_in_completed);
#endif
        m8_usb_diag.rx_ep2_bytes += (uint32_t)len;
        for (int i = 0; i < len; ++i) rx_push(data[i]);
    }
}

/* IN completion callback: fsusb.c clears EP3 busy before calling this. */
int HandleInRequest(struct _USBState *ctx, int endpoint, uint8_t *data, int len)
{
    (void)ctx;
    if (endpoint == CDC_IN_EP) {
        m8_usb_diag.ep3_in_completed++;
#ifndef M8_HOST_TEST
        /* Called from fsusb IRQ BEFORE it clears endpoints[3].busy. */
        M8D_TraceIRQ(M8D_EV_EP3_IRQ_IN, (uint32_t)len,
            m8_usb_diag.tx_accepted, m8_usb_diag.ep3_in_completed);
#endif
    }
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
#ifndef M8_HOST_TEST
    M8D_TraceIRQ(M8D_EV_CTRL_SETUP, (uint32_t)(uint8_t)setup_code,
        m8_usb_diag.tx_accepted, m8_usb_diag.ep3_in_completed);
#endif
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
#define M8_MAX_COMMAND_LENGTH 31u
#define M8_REPLY_CAPACITY 192u

static uint8_t command[M8_MAX_COMMAND_LENGTH + 1u];
static uint8_t command_length;
static uint8_t command_too_long;
static uint8_t command_invalid;
static uint32_t command_errors;

static uint8_t reply[M8_REPLY_CAPACITY];
static uint16_t reply_length;
static uint16_t reply_sent;

static void M8_AppendChar(char c)
{
    /* Reserve space for the terminal newline. */
    if (reply_length < M8_REPLY_CAPACITY - 1u)
        reply[reply_length++] = (uint8_t)c;
}
static void M8_AppendText(const char *p)
{
    while (*p) M8_AppendChar(*p++);
}
static void M8_AppendU32(uint32_t value)
{
    char digits[10];
    uint8_t n = 0;
    do {
        digits[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value && n < sizeof(digits));
    while (n) M8_AppendChar(digits[--n]);
}
/* Safe for INT32_MIN: unsigned subtraction avoids signed negation overflow. */
static void M8_AppendI32(int32_t value)
{
    if (value < 0) {
        M8_AppendChar('-');
        M8_AppendU32(0u - (uint32_t)value);
    } else {
        M8_AppendU32((uint32_t)value);
    }
}
static void M8_FinishReply(void)
{
    reply[reply_length++] = (uint8_t)'\n';
    reply_sent = 0;
}
static int M8_CommandIs(const char *candidate)
{
    uint8_t i = 0;
    while (candidate[i] && i < command_length) {
        if ((uint8_t)candidate[i] != command[i]) return 0;
        ++i;
    }
    return i == command_length && candidate[i] == '\0';
}
static void M8_BuildResponse(void)
{
    reply_length = 0;
    reply_sent = 0;
    if (command_too_long) {
        ++command_errors;
        M8_AppendText("ERR line_too_long");
    } else if (command_invalid) {
        ++command_errors;
        M8_AppendText("ERR invalid_character");
    } else if (M8_CommandIs("INFO")) {
        M8_AppendText("INFO board=CH32X035F8U6 firmware=m8e_usb_cdc_irq_trace protocol=5");
    } else if (M8_CommandIs("STATUS")) {
        M8_AppendText("STATUS rx_packets=");
        M8_AppendU32(m8_usb_diag.rx_ep2_packets);
        M8_AppendText(" rx_bytes=");
        M8_AppendU32(m8_usb_diag.rx_ep2_bytes);
        M8_AppendText(" tx_packets=");
        M8_AppendU32(m8_usb_diag.tx_accepted);
        M8_AppendText(" tx_bytes=");
        M8_AppendU32(m8_usb_diag.tx_bytes_accepted);
        M8_AppendText(" rx_overflows=");
        M8_AppendU32(rx_overflows);
        M8_AppendText(" errors=");
        M8_AppendU32(command_errors);
    } else if (M8_CommandIs("MEASURE")) {
        M8_Measurement sample;
        /* Only the measurement backend may claim validity of real values. */
        M8_ReadMeasurement(&sample);
        if (sample.status == M8_MEASURE_OK) {
            M8_AppendText("MEASURE status=ok voltage_mV=");
            M8_AppendU32(sample.voltage_mV);
            M8_AppendText(" current_mA=");
            M8_AppendI32(sample.current_mA);
        } else if (sample.status == M8_MEASURE_NOT_CONFIGURED) {
            M8_AppendText("MEASURE status=unavailable reason=not_configured voltage_mV=NA current_mA=NA");
        } else if (sample.status == M8_MEASURE_IO_ERROR) {
            M8_AppendText("MEASURE status=error reason=sensor_io voltage_mV=NA current_mA=NA");
        } else {
            M8_AppendText("MEASURE status=error reason=backend_invalid voltage_mV=NA current_mA=NA");
        }
    } else if (M8_CommandIs("PING")) {
        M8_AppendText("PONG");
    } else if (M8_CommandIs("HELP")) {
        M8_AppendText("HELP INFO STATUS MEASURE PING HELP");
    } else {
        ++command_errors;
        M8_AppendText("ERR unknown_command");
    }
    M8_FinishReply();
    command_length = 0;
    command_too_long = 0;
    command_invalid = 0;
}
static void M8_ConsumeByte(uint8_t ch)
{
    if (ch == (uint8_t)'\r') return;
    if (ch == (uint8_t)'\n') {
        M8_BuildResponse();
        return;
    }
    if (ch < 0x20u || ch > 0x7Eu) command_invalid = 1;
    if (command_length < M8_MAX_COMMAND_LENGTH)
        command[command_length++] = ch;
    else
        command_too_long = 1;
}

/* Called repeatedly from main. At most one USB IN submit or one RX byte
 * is processed per iteration; failed submits do not discard response data.
 */
void M8_ServiceUsbCommands(void)
{
#ifndef M8_HOST_TEST
    /* Poll on EVERY foreground iteration, even when the reply has been submitted.
       M8 missed short freezes because it polled only while a reply was pending. */
    M8D_TracePoll(m8_usb_diag.tx_accepted, m8_usb_diag.ep3_in_completed);
#endif
    if (reply_sent < reply_length) {
        uint8_t *ep_buffer = USBFS_GetEPBufferIfAvailable(CDC_IN_EP);
        if (ep_buffer == NULL) {
            M8_DiagSampleBusy();
            return;
        }
        uint16_t remain = (uint16_t)(reply_length - reply_sent);
        uint8_t len = (uint8_t)(remain > CDC_TX_CHUNK_SIZE ? CDC_TX_CHUNK_SIZE : remain);
        for (uint8_t i = 0; i < len; ++i)
            ep_buffer[i] = reply[reply_sent + i];
        m8_usb_diag.tx_attempts++;
        m8_usb_diag.tx_last_len = len;
#ifndef M8_HOST_TEST
        M8D_TraceFG(M8D_EV_TX_BEGIN, (uint32_t)len,
            m8_usb_diag.tx_accepted, m8_usb_diag.ep3_in_completed);
#endif
        int rc = USBFS_SendEndpoint(CDC_IN_EP, len);
        m8_usb_diag.tx_last_rc = rc;
        if (rc == 0) {
            m8_usb_diag.tx_accepted++;
            m8_usb_diag.tx_bytes_accepted += len;
            reply_sent = (uint16_t)(reply_sent + len);
#ifndef M8_HOST_TEST
            M8D_TraceFG(M8D_EV_TX_ACCEPTED, (uint32_t)len,
                m8_usb_diag.tx_accepted, m8_usb_diag.ep3_in_completed);
#endif
        } else {
            m8_usb_diag.tx_rejected++;
#ifndef M8_HOST_TEST
            M8D_TraceFG(M8D_EV_TX_REJECTED, (uint32_t)(int32_t)rc,
                m8_usb_diag.tx_accepted, m8_usb_diag.ep3_in_completed);
#endif
            if (rc == -1) m8_usb_diag.tx_reject_minus1++;
            if (rc == -2) m8_usb_diag.tx_reject_minus2++;
            if (rc == -3) m8_usb_diag.tx_reject_minus3++;
        }
        return;
    }
    if (rx_read_idx != rx_write_idx) {
        uint8_t ch = rx_ring[rx_read_idx];
        rx_read_idx = (uint8_t)(rx_read_idx + 1u);
        M8_ConsumeByte(ch);
    }
}

#ifndef M8_HOST_TEST
int main(void)
{
    SystemInit();
    m8_usb_diag.magic = 0x4d384447u; /* retain M8DG for compatibility */
    m8_usb_diag.version = 1u;
    M8D_TraceInit();
    M8E_IrqTraceInit();
    USBFSSetup();
    for (;;) M8_ServiceUsbCommands();
}
#endif
