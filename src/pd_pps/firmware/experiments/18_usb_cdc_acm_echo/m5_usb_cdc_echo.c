/*
 * M5 USB CDC-ACM echo experiment for CH32X035 / ch32fun.
 * Copyright (c) 2026. Released under the MIT license (see LICENSE).
 *
 * EP0: control, EP1 IN: CDC notification, EP2 OUT: host->device,
 * EP3 IN: device->host. The descriptor comes from the vendored ch32fun
 * USBFS CDC TTY example, copied into this experiment as usb_config.h.
 *
 * No USB-PD negotiation and no change to Source VBUS are performed here.
 */
#include <stdint.h>
#include <stddef.h>

#ifdef M5_HOST_TEST
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
#define ECHO_BUFFER_SIZE 256u
/* EP2 OUT accepts full-speed bulk packets of up to 64 bytes. */
#define CDC_OUT_MAX_PACKET_SIZE 64u
/* EP3 IN intentionally uses short packets (<64 bytes) to terminate each
 * transfer without a zero-length packet (ZLP). This is conservative and
 * suitable for the M5 control/monitoring experiment; optimize later if needed.
 */
#define ECHO_TX_CHUNK_SIZE 63u

/* uint8_t indices wrap naturally at 256; capacity is 255 bytes. */
static volatile uint8_t rx_ring[ECHO_BUFFER_SIZE];
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
 * M5 USBFS diagnostic snapshot (version 1): RAM-only, no CDC output.
 * All fields are 32-bit words for straightforward SRAM/ELF decoding.
 * ISR counters and foreground counters have independent writers.
 * Snapshot register fields are sampled by the MCU itself, NOT by WCH-LinkE.
 */
typedef struct {
    uint32_t magic;               /* 0x4D354447 = M5DG */
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
} M5_UsbDiagnostics;

/* 'used' ensures the object survives link-time optimization for RAM capture. */
volatile M5_UsbDiagnostics m5_usb_diag __attribute__((used));

static void M5_DiagSampleBusy(void)
{
    /* Rate limit MMIO sampling in the tight foreground poll loop. */
    m5_usb_diag.ep3_busy_polls++;
    if ((m5_usb_diag.ep3_busy_polls & 0x3fffu) != 0u) return;
    m5_usb_diag.ep3_busy_snapshots++;
#ifndef M5_HOST_TEST
    m5_usb_diag.busy_hw_base_ctrl = USBFS->BASE_CTRL;
    m5_usb_diag.busy_hw_udev_ctrl = USBFS->UDEV_CTRL;
    m5_usb_diag.busy_hw_int_en = USBFS->INT_EN;
    m5_usb_diag.busy_hw_int_fg = USBFS->INT_FG;
    m5_usb_diag.busy_hw_int_st = USBFS->INT_ST;
    m5_usb_diag.busy_hw_uep2_3_mod = USBFS->UEP2_3_MOD;
    m5_usb_diag.busy_hw_ep3_tx_len = UEP_CTRL_LEN(CDC_IN_EP);
    m5_usb_diag.busy_hw_ep3_tx_ctrl = UEP_CTRL_TX(CDC_IN_EP);
    m5_usb_diag.busy_ctx_ep3_busy = USBFSCTX.endpoints[CDC_IN_EP].busy;
    m5_usb_diag.busy_ctx_ep3_mode = USBFSCTX.endpoints[CDC_IN_EP].mode;
    m5_usb_diag.busy_ctx_setup_len = USBFSCTX.USBFS_SetupReqLen;
    m5_usb_diag.busy_ctx_errata_window = USBFSCTX.USBFS_errata_dont_send_endpoint_in_window;
#endif
}

static void echo_push(uint8_t byte)
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
        m5_usb_diag.rx_ep2_packets++;
        m5_usb_diag.rx_ep2_bytes += (uint32_t)len;
        for (int i = 0; i < len; ++i) echo_push(data[i]);
    }
}

/* IN endpoint is serviced with USBFS_SendEndpointNEW in the foreground. */
int HandleInRequest(struct _USBState *ctx, int endpoint, uint8_t *data, int len)
{
    (void)ctx;
    if (endpoint == CDC_IN_EP) m5_usb_diag.ep3_in_completed++;
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
 * Nonblocking foreground forwarding. Leaves bytes queued when EP3 is busy.
 *
 * IMPORTANT: USBFS_SendEndpointNEW() in the project's pinned ch32fun revision
 * enables the USB IN endpoint (ACK) before setting endpoints[ep].busy = 1,
 * without masking USB_IRQn. If an IN completion IRQ occurs in that window,
 * it can clear busy to 0 before NEW sets it back to 1, deadlocking EP3.
 *
 * USBFS_SendEndpoint() masks USB_IRQn while setting ACK and busy. Put data
 * directly into the endpoint-owned DMA buffer while it is idle, then use
 * this IRQ-protected sender. No shared library files need modification.
 */
void M5_ServiceUsbEcho(void)
{
    uint8_t *ep_buffer = USBFS_GetEPBufferIfAvailable(CDC_IN_EP);
    if (ep_buffer == NULL) {
        M5_DiagSampleBusy();
        return;
    }

    uint8_t count = 0;
    const uint8_t start = rx_read_idx;
    const uint8_t write_snapshot = rx_write_idx;
    while (count < ECHO_TX_CHUNK_SIZE &&
           (uint8_t)(start + count) != write_snapshot) {
        ep_buffer[count] = rx_ring[(uint8_t)(start + count)];
        ++count;
    }
    if (count == 0) return;

    /* Consume RX data only after the endpoint accepts this packet. */
    m5_usb_diag.tx_attempts++;
    m5_usb_diag.tx_last_len = count;
    int rc = USBFS_SendEndpoint(CDC_IN_EP, count);
    m5_usb_diag.tx_last_rc = rc;
    if (rc == 0) {
        m5_usb_diag.tx_accepted++;
        m5_usb_diag.tx_bytes_accepted += count;
        rx_read_idx = (uint8_t)(start + count);
    } else {
        m5_usb_diag.tx_rejected++;
        if (rc == -1) m5_usb_diag.tx_reject_minus1++;
        if (rc == -2) m5_usb_diag.tx_reject_minus2++;
        if (rc == -3) m5_usb_diag.tx_reject_minus3++;
    }
}

#ifndef M5_HOST_TEST
int main(void)
{
    SystemInit();
    m5_usb_diag.magic = 0x4d354447u; /* M5DG */
    m5_usb_diag.version = 1u;
    USBFSSetup();
    for (;;) {
        M5_ServiceUsbEcho();
    }
}
#endif
