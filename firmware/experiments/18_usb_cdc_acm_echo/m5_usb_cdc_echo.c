/*
 * M5 USB CDC-ACM echo experiment for CH32X035 / ch32fun.
 * Copyright (c) 2026. Released under the MIT license (see LICENSE).
 *
 * EP0: control, EP1 IN: CDC notification, EP2 OUT: host->device,
 * EP3 IN: device->host. The descriptor comes from the vendored ch32fun
 * USBFS CDC TTY example, via the include path in Makefile.
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
#define ECHO_PACKET_SIZE 64u

/* uint8_t indices wrap naturally at 256; capacity is 255 bytes. */
static volatile uint8_t rx_ring[ECHO_BUFFER_SIZE];
static volatile uint8_t rx_write_idx;
static volatile uint8_t rx_read_idx;
static volatile uint32_t rx_overflows;
static uint8_t tx_packet[ECHO_PACKET_SIZE];

/* 115200 8N1 is a CDC line coding convention, not a physical UART baud. */
static uint8_t cdc_line_coding[7] = {
    0x00, 0xC2, 0x01, 0x00, /* dwDTERate: 115200 */
    0x00,                   /* one stop bit */
    0x00,                   /* no parity */
    0x08                    /* 8 data bits */
};
static volatile uint8_t expecting_line_coding;

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
    if (endpoint == CDC_OUT_EP && len > 0 && len <= (int)ECHO_PACKET_SIZE) {
        for (int i = 0; i < len; ++i) echo_push(data[i]);
    }
}

/* IN endpoint is serviced with USBFS_SendEndpointNEW in the foreground. */
int HandleInRequest(struct _USBState *ctx, int endpoint, uint8_t *data, int len)
{
    (void)ctx;
    (void)endpoint;
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

/* Nonblocking foreground forwarding. Leaves bytes queued when EP3 is busy. */
void M5_ServiceUsbEcho(void)
{
    uint8_t count = 0;
    const uint8_t start = rx_read_idx;
    const uint8_t write_snapshot = rx_write_idx;
    while (count < ECHO_PACKET_SIZE &&
           (uint8_t)(start + count) != write_snapshot) {
        tx_packet[count] = rx_ring[(uint8_t)(start + count)];
        ++count;
    }
    if (count == 0) return;

    if (USBFS_SendEndpointNEW(CDC_IN_EP, tx_packet, count, 1) == 0) {
        rx_read_idx = (uint8_t)(start + count);
    }
}

#ifndef M5_HOST_TEST
int main(void)
{
    SystemInit();
    USBFSSetup();
    for (;;) {
        M5_ServiceUsbEcho();
    }
}
#endif
