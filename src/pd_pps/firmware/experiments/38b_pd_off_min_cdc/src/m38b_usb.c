/* M38B USBPD-disabled control: same CDC and USBFS lower layer as M38A.
 * No PD firmware is compiled or linked, no PD communication or PPS request.
 * Only USBFS interrupt counters and CDC PING/STAT/COUNT are active. */
#include "m38b_usb.h"
#include "ch32fun.h"
#include "fsusb.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CDC_OUT_EP 2
#define CDC_IN_EP  3
#define CDC_SET_CONTROL_LINE_STATE 0x22u
#define CDC_CLASS_INTERFACE_OUT  0x21u
#define CDC_CLASS_INTERFACE_IN   0xA1u
#define M38_RX_SIZE 128u /* Power of two, one empty slot. */
#define M38_TX_SIZE 144u

/* ISR producer / foreground consumer. IRQ never blocks or formats text. */
static volatile uint8_t rx_head, rx_tail;
static uint8_t rx_buffer[M38_RX_SIZE];
static volatile uint32_t irq_count, setup_count, in_count, out_count;
static volatile uint32_t reset_count, rx_overflow;
static uint8_t line_coding[7] = {0x00,0xC2,0x01,0x00,0x00,0x00,0x08};
static volatile uint8_t expect_line_coding;

/* USB events are observed without modifying the interrupt handling logic. */
void m38b_usb_on_irq(uint16_t intfgst)
{
    irq_count++;
    if (intfgst & CRB_UIF_TRANSFER) {
        switch ((intfgst & CMASK_UIS_TOKEN) >> 12) {
            case CUIS_TOKEN_SETUP: setup_count++; break;
            case CUIS_TOKEN_IN:    in_count++; break;
            case CUIS_TOKEN_OUT:   out_count++; break;
            default: break;
        }
    }
}
void m38b_usb_on_bus_reset_irq(void) { reset_count++; }

void HandleDataOut(struct _USBState *ctx, int endpoint, uint8_t *data, int len)
{
    if (endpoint == 0) {
        if (expect_line_coding && len == (int)sizeof(line_coding)) {
            for (unsigned i = 0; i < sizeof(line_coding); ++i)
                line_coding[i] = data[i];
        }
        expect_line_coding = 0;
        ctx->USBFS_SetupReqLen = 0;
    } else if (endpoint == CDC_OUT_EP && len > 0 && len <= 64) {
        for (int i = 0; i < len; ++i) {
            uint8_t next = (uint8_t)((rx_head + 1u) & (M38_RX_SIZE - 1u));
            if (next == rx_tail) {
                rx_overflow++;
                break;
            }
            rx_buffer[rx_head] = data[i];
            rx_head = next;
        }
    }
}
int HandleInRequest(struct _USBState *ctx, int endpoint, uint8_t *data, int len)
{
    (void)ctx; (void)endpoint; (void)data; (void)len;
    return 0;
}
int HandleSetupCustom(struct _USBState *ctx, int setup_code)
{
    ctx->pCtrlPayloadPtr = NULL;
    expect_line_coding = 0;
    switch ((unsigned)setup_code) {
    case CDC_SET_LINE_CODING:
        if (ctx->USBFS_SetupReqType != CDC_CLASS_INTERFACE_OUT ||
            ctx->USBFS_SetupReqLen != sizeof(line_coding)) return 0;
        expect_line_coding = 1;
        return (int)sizeof(line_coding);
    case CDC_GET_LINE_CODING:
        if (ctx->USBFS_SetupReqType != CDC_CLASS_INTERFACE_IN ||
            ctx->USBFS_SetupReqLen == 0) return 0;
        ctx->pCtrlPayloadPtr = line_coding;
        return ctx->USBFS_SetupReqLen < sizeof(line_coding)
            ? (int)ctx->USBFS_SetupReqLen : (int)sizeof(line_coding);
    case CDC_SET_CONTROL_LINE_STATE:
    case CDC_SEND_BREAK:
        if (ctx->USBFS_SetupReqType != CDC_CLASS_INTERFACE_OUT ||
            ctx->USBFS_SetupReqLen != 0) return 0;
        return -1; /* Status stage ACK. */
    default: return 0;
    }
}

typedef struct {
    uint32_t setup, in, out, reset, irq;
} counters_t;

static char line[24];
static uint8_t line_len;
static char response[M38_TX_SIZE];
static uint8_t response_len, response_offset;

static counters_t counts(void)
{
    counters_t c;
    /* 32-bit aligned counters are atomic on RV32. */
    c.setup = setup_count;
    c.in = in_count;
    c.out = out_count;
    c.reset = reset_count;
    c.irq = irq_count;
    return c;
}
static bool rx_pop(uint8_t *p)
{
    if (rx_tail == rx_head) return false;
    *p = rx_buffer[rx_tail];
    rx_tail = (uint8_t)((rx_tail + 1u) & (M38_RX_SIZE - 1u));
    return true;
}
static void set_response(const char *command)
{
    int n = 0;
    if (strcmp(command, "PING") == 0) {
        n = snprintf(response, sizeof(response), "PONG M38B\r\n");
    } else if (strcmp(command, "STAT") == 0) {
        n = snprintf(response, sizeof(response),
            "STAT pd=off pps=0 cfg=%u\r\n",
            (unsigned)USBFSCTX.USBFS_DevConfig);
    } else if (strcmp(command, "COUNT") == 0) {
        counters_t c = counts();
        n = snprintf(response, sizeof(response),
            "COUNT set=%lu in=%lu out=%lu rst=%lu irq=%lu ov=%lu\r\n",
            (unsigned long)c.setup, (unsigned long)c.in,
            (unsigned long)c.out, (unsigned long)c.reset,
            (unsigned long)c.irq, (unsigned long)rx_overflow);
    } else {
        n = snprintf(response, sizeof(response), "ERR try PING STAT COUNT\r\n");
    }
    response_offset = 0;
    if (n <= 0) response_len = 0;
    else if ((size_t)n >= sizeof(response)) response_len = (uint8_t)(sizeof(response) - 1u);
    else response_len = (uint8_t)n;
}
static void send_one_packet(void)
{
    if (response_offset >= response_len || !USBFSCTX.USBFS_DevConfig) return;
    uint8_t *epbuf = USBFS_GetEPBufferIfAvailable(CDC_IN_EP);
    if (!epbuf) return;
    uint8_t remaining = (uint8_t)(response_len - response_offset);
    int length = remaining > 60u ? 60 : remaining; /* Never need a ZLP. */
    memcpy(epbuf, response + response_offset, (size_t)length);
    if (USBFS_SendEndpoint(CDC_IN_EP, length) == 0)
        response_offset = (uint8_t)(response_offset + (uint8_t)length);
}
void m38b_usb_init(void)
{
    rx_head = 0;
    rx_tail = 0;
    line_len = 0;
    response_len = response_offset = 0;
    expect_line_coding = 0;
    (void)USBFSSetup();
}
void m38b_usb_service(void)
{
    /* No reset, detach or USBFSSetup here. M38B changes only PD state
       relative to M38A; USBFS init and endpoint handling are preserved. */
    if (!USBFSCTX.USBFS_DevConfig) {
        rx_tail = rx_head;
        line_len = response_len = response_offset = 0;
        return;
    }
    if (response_offset < response_len) {
        send_one_packet();
        return;
    }
    response_len = response_offset = 0;
    for (unsigned i = 0; i < 16; i++) {
        uint8_t b;
        if (!rx_pop(&b)) break;
        if (b == '\r' || b == '\n') {
            if (line_len) {
                line[line_len] = 0;
                set_response(line);
                line_len = 0;
                break;
            }
        } else if (b >= 0x20 && b <= 0x7e) {
            if (line_len + 1u < sizeof(line))
                line[line_len++] = (char)b;
            else line_len = 0; /* Bounded parser; malformed long command. */
        }
    }
    send_one_packet();
}
