/* M39 (M38A derivative): minimal read-only CDC application with the same USBFS low-level driver.
 * Preserve the PD17 policy/PHY/safety path. No active USB recovery, no
 * asynchronous printf, and no PD calls from USB IRQ callbacks. */
#include "m38_usb.h"
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
#define M38_TX_SIZE 192u

/* ISR producer / foreground consumer. IRQ never blocks or formats text. */
static volatile uint8_t rx_head, rx_tail;
static uint8_t rx_buffer[M38_RX_SIZE];
static volatile uint32_t irq_count, setup_count, in_count, out_count;
static volatile uint32_t reset_count, rx_overflow;
static uint8_t line_coding[7] = {0x00,0xC2,0x01,0x00,0x00,0x00,0x08};
static volatile uint8_t expect_line_coding;

/* USB events are observed without modifying the interrupt handling logic. */
void m38_usb_on_irq(uint16_t intfgst)
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
void m38_usb_on_bus_reset_irq(void) { reset_count++; }

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
static counters_t start, during;
static uint32_t on_ms, duration_ms;
static bool last_attached, has_duration;
static uint8_t last_pe, last_pps;
static uint32_t last_mv, last_ma;
static bool now_attached;
/* Foreground-only pointers; PD modules own the session snapshots.
 * No PD function call, register access or I/O is added to the USB IRQ. */
static const pd_stability_t *diag_stability;
static const pd_stop_diag_t *diag_stop;
static uint32_t diag_faults;

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
        n = snprintf(response, sizeof(response), "PONG M39\r\n");
    } else if (strcmp(command, "STAT") == 0) {
        /* Keep this message read-only and independent of sensor/I2C access. */
        n = snprintf(response, sizeof(response),
            "STAT a=%u pe=%u pps=%u mv=%lu ma=%lu cfg=%u\r\n",
            (unsigned)now_attached, (unsigned)last_pe, (unsigned)last_pps,
            (unsigned long)last_mv, (unsigned long)last_ma,
            (unsigned)USBFSCTX.USBFS_DevConfig);
    } else if (strcmp(command, "COUNT") == 0) {
        counters_t c = counts();
        n = snprintf(response, sizeof(response),
            "COUNT set=%lu in=%lu out=%lu rst=%lu irq=%lu ov=%lu\r\n",
            (unsigned long)c.setup, (unsigned long)c.in,
            (unsigned long)c.out, (unsigned long)c.reset,
            (unsigned long)c.irq, (unsigned long)rx_overflow);
    } else if (strcmp(command, "DELTA") == 0) {
        /* Before detach the saved end snapshot is undefined; never present
         * unsigned underflows as meaningful USB event counts. */
        n = snprintf(response, sizeof(response),
            "DELTA valid=%u ms=%lu set=%lu in=%lu out=%lu rst=%lu\r\n",
            (unsigned)has_duration,
            (unsigned long)(has_duration ? duration_ms : 0u),
            (unsigned long)(has_duration ? during.setup - start.setup : 0u),
            (unsigned long)(has_duration ? during.in - start.in : 0u),
            (unsigned long)(has_duration ? during.out - start.out : 0u),
            (unsigned long)(has_duration ? during.reset - start.reset : 0u));
    } else if (strcmp(command, "PDSTOP") == 0) {
        const pd_stop_diag_t *d = diag_stop;
        n = snprintf(response, sizeof(response),
            "PDSTOP active=%u found=%u reason=%u ms=%lu before=%u after=%u aux=%u hr=%lu detach=%u after_ms=%lu\r\n",
            d ? (unsigned)d->active : 0u,
            d ? (unsigned)d->found : 0u,
            d ? (unsigned)d->reason : 0u,
            d ? (unsigned long)d->stop_rel_ms : 0ul,
            d ? (unsigned)d->pe_before : 0u,
            d ? (unsigned)d->pe_after : 0u,
            d ? (unsigned)d->aux : 0u,
            d ? (unsigned long)d->hr_rx_count : 0ul,
            d ? (unsigned)d->detach_seen : 0u,
            d ? (unsigned long)d->detach_after_stop_ms : 0ul);
    } else if (strcmp(command, "PDSTAB") == 0) {
        const pd_stability_t *s = diag_stability;
        n = snprintf(response, sizeof(response),
            "PDSTAB active=%u elapsed=%lu ready=%lu caps=%lu req=%lu pps=%lu fixed=%lu ref_start=%lu ref_done=%lu\r\n",
            s ? (unsigned)s->active : 0u,
            s ? (unsigned long)s->elapsed_ms : 0ul,
            s ? (unsigned long)s->ready_ms : 0ul,
            s ? (unsigned long)s->caps_received : 0ul,
            s ? (unsigned long)s->requests_issued : 0ul,
            s ? (unsigned long)s->pps_ready : 0ul,
            s ? (unsigned long)s->fixed_ready : 0ul,
            s ? (unsigned long)s->refresh_started : 0ul,
            s ? (unsigned long)s->refresh_completed : 0ul);
    } else if (strcmp(command, "PDERR") == 0) {
        const pd_stability_t *s = diag_stability;
        n = snprintf(response, sizeof(response),
            "PDERR hr=%lu recovery=%lu vbus_loss=%lu adc_fault=%lu faults=%lu trace_drop=%lu event_lost=%lu\r\n",
            s ? (unsigned long)s->source_hr_rx : 0ul,
            s ? (unsigned long)s->recovery_entries : 0ul,
            s ? (unsigned long)s->vbus_loss : 0ul,
            s ? (unsigned long)s->adc_fault : 0ul,
            (unsigned long)diag_faults,
            s ? (unsigned long)s->trace_drops_delta : 0ul,
            s ? (unsigned long)s->lost_delta : 0ul);
    } else {
        n = snprintf(response, sizeof(response),
            "ERR try PING STAT COUNT DELTA PDSTOP PDSTAB PDERR\r\n");
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
void m38_usb_init(void)
{
    rx_head = 0;
    rx_tail = 0;
    line_len = 0;
    response_len = response_offset = 0;
    last_attached = false;
    has_duration = false;
    diag_stability = NULL;
    diag_stop = NULL;
    diag_faults = 0;
    expect_line_coding = 0;
    (void)USBFSSetup();
}
void m38_usb_service(uint32_t now_ms, const pe_t *pe, bool attached,
                     const pd_stability_t *stability,
                     const pd_stop_diag_t *stop)
{
    /* Main loop already updated these records. Read them only in foreground. */
    diag_stability = stability;
    diag_stop = stop;
    /* pd_stability_detach() saved the policy faults before PE_DETACH
     * can reset them. Expose that saved count after physical detach. */
    diag_faults = stability && !stability->active && stability->elapsed_ms
        ? stability->faults_at_end : (pe ? pe->faults : 0u);
    /* Only snapshots in foreground, never PD functions from the USB ISR. */
    now_attached = attached;
    last_pe = pe ? (uint8_t)pe->state : 0;
    last_pps = pe ? (uint8_t)pe->pps_active : 0;
    last_mv = pe ? pe->requested_mv : 0;
    last_ma = pe ? pe->requested_ma : 0;
    if (attached != last_attached) {
        counters_t c = counts();
        if (attached) {
            start = c;
            on_ms = now_ms;
            has_duration = false;
        } else {
            during = c;
            duration_ms = now_ms - on_ms;
            has_duration = true;
        }
        last_attached = attached;
    }
    /* No reset, detach or USBFSSetup from here: the experimental variable
       is the CDC application layer, NOT the PD/USBFS hardware setup. */
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
