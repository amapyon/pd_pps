/* M8D USBFS EP3 trace: independent experiment, SRAM readout only.
 * See README for capture semantics and limitations.
 * Copyright (c) 2026. Released under MIT license (LICENSE).
 */
#include "m8d_trace.h"
#ifdef M8D_TRACE_HOST_TEST
#include "host_test/mock_trace_usb.h"
#else
#include "ch32fun.h"
#include "fsusb.h"
#endif

volatile M8D_UsbTrace m8d_usb_trace __attribute__((used));

typedef struct {
    uint32_t usb_flags;
    uint32_t ep3_regs;
    uint32_t ctx_flags;
} M8D_HardwareSnapshot;

static M8D_HardwareSnapshot M8D_ReadHardware(void)
{
    M8D_HardwareSnapshot s;
    s.usb_flags = (uint32_t)USBFS->INT_FG |
        ((uint32_t)USBFS->INT_ST << 8u) |
        ((uint32_t)USBFS->BASE_CTRL << 16u) |
        ((uint32_t)USBFS->UDEV_CTRL << 24u);
    s.ep3_regs = (uint32_t)UEP_CTRL_LEN(3) |
        ((uint32_t)UEP_CTRL_TX(3) << 8u) |
        ((uint32_t)USBFS->INT_EN << 16u) |
        ((uint32_t)USBFS->UEP2_3_MOD << 24u);
    s.ctx_flags = (uint32_t)USBFSCTX.endpoints[3].busy |
        ((uint32_t)USBFSCTX.endpoints[3].mode << 8u) |
        ((uint32_t)USBFSCTX.USBFS_SetupReqLen << 16u) |
        ((uint32_t)(USBFSCTX.USBFS_errata_dont_send_endpoint_in_window != 0u) << 31u);
    return s;
}

void M8D_TraceInit(void)
{
    /* BSS starts zero; never clear trace after USBFSSetup or on timeout. */
    m8d_usb_trace.magic = M8D_TRACE_MAGIC;
    m8d_usb_trace.version = M8D_TRACE_VERSION;
    m8d_usb_trace.struct_size = sizeof(M8D_UsbTrace);
    m8d_usb_trace.event_size = sizeof(M8D_TraceEvent);
    m8d_usb_trace.capacity = M8D_TRACE_CAPACITY;
}

static void M8D_SaveEvent(volatile M8D_TraceEvent *record, uint32_t sequence,
                          uint32_t event, uint32_t arg,
                          uint32_t tx_accepted, uint32_t ep3_in_completed,
                          M8D_HardwareSnapshot s)
{
    record->sequence = sequence;
    record->event = event;
    record->main_loops = m8d_usb_trace.main_loops;
    record->arg = arg;
    record->usb_flags = s.usb_flags;
    record->ep3_regs = s.ep3_regs;
    record->ctx_flags = s.ctx_flags;
    record->tx_accepted = tx_accepted;
    record->ep3_in_completed = ep3_in_completed;
}

void M8D_TraceFG(uint32_t event, uint32_t arg,
                 uint32_t tx_accepted, uint32_t ep3_in_completed)
{
    const uint32_t next = m8d_usb_trace.fg_count + 1u;
    const uint32_t index = m8d_usb_trace.fg_count & (M8D_TRACE_CAPACITY - 1u);
    const M8D_HardwareSnapshot s = M8D_ReadHardware();
    M8D_SaveEvent(&m8d_usb_trace.fg_events[index], next,
                  event, arg, tx_accepted, ep3_in_completed, s);
    m8d_usb_trace.fg_count = next;
}

void M8D_TraceIRQ(uint32_t event, uint32_t arg,
                  uint32_t tx_accepted, uint32_t ep3_in_completed)
{
    const uint32_t next = m8d_usb_trace.irq_count + 1u;
    const uint32_t index = m8d_usb_trace.irq_count & (M8D_TRACE_CAPACITY - 1u);
    const M8D_HardwareSnapshot s = M8D_ReadHardware();
    M8D_SaveEvent(&m8d_usb_trace.irq_events[index], next,
                  event, arg, tx_accepted, ep3_in_completed, s);
    m8d_usb_trace.irq_count = next;
}

/* Always called, including after the final packet of a reply was accepted.
 * No automatic USB recovery: preserve fault state for debugger capture.
 * The foreground loop counter is NOT a clock. Repetitions are calibrated
 * by neither instruction timing nor CPU frequency. */
void M8D_TracePoll(uint32_t tx_accepted, uint32_t ep3_in_completed)
{
    m8d_usb_trace.main_loops++;
    const uint32_t busy = (uint32_t)USBFSCTX.endpoints[3].busy;
    if (tx_accepted != ep3_in_completed && busy) {
        uint32_t ticks = ++m8d_usb_trace.outstanding_iterations;
        if (ticks > m8d_usb_trace.max_outstanding_iterations)
            m8d_usb_trace.max_outstanding_iterations = ticks;
        /* Event-based snapshots survive even when no further CDC commands
         * are received. The last live state refreshes every 4096 loops. */
        if (ticks == 1u || ticks == 256u || ticks == 4096u ||
            ticks == 65536u || ticks == 1048576u || ticks == 16777216u) {
            M8D_TraceFG(M8D_EV_WAIT_SAMPLE, ticks, tx_accepted, ep3_in_completed);
        }
        if (ticks == 1u || (ticks & 0xfffu) == 0u) {
            M8D_HardwareSnapshot s = M8D_ReadHardware();
            m8d_usb_trace.last_usb_flags = s.usb_flags;
            m8d_usb_trace.last_ep3_regs = s.ep3_regs;
            m8d_usb_trace.last_ctx_flags = s.ctx_flags;
            m8d_usb_trace.last_tx_accepted = tx_accepted;
            m8d_usb_trace.last_ep3_in_completed = ep3_in_completed;
        }
    } else if (m8d_usb_trace.outstanding_iterations != 0u) {
        const uint32_t ended = m8d_usb_trace.outstanding_iterations;
        m8d_usb_trace.outstanding_iterations = 0u;
        M8D_TraceFG(M8D_EV_WAIT_END, ended, tx_accepted, ep3_in_completed);
    }
}
