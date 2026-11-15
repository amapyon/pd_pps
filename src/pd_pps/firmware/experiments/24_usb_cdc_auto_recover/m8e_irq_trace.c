/* M8E independent USBFS IRQ-entry diagnostics.
 * Captures pre-handler flags only; never clears flags, toggles bits or retries.
 * The local m8e_fsusb.c calls M8E_IrqEntry() immediately after reading
 * INT_FG+INT_ST in the upstream USBFS IRQ handler.
 * Copyright (c) 2026. Released under MIT license (see LICENSE).
 */
#include "m8e_irq_trace.h"
#include "m8d_trace.h"
#include "m8f_recovery.h"
#ifdef M8E_IRQ_HOST_TEST
#include "host_test/mock_irq_usb.h"
#else
#include "ch32fun.h"
#include "fsusb.h"
#endif

volatile M8E_IrqTrace m8e_irq_trace __attribute__((used));
void M8E_IrqTraceInit(void)
{
    m8e_irq_trace.magic = M8E_IRQ_MAGIC;
    m8e_irq_trace.version = M8E_IRQ_VERSION;
    m8e_irq_trace.struct_size = sizeof(M8E_IrqTrace);
    m8e_irq_trace.event_size = sizeof(M8E_IrqEvent);
    m8e_irq_trace.capacity = M8E_IRQ_CAPACITY;
}
void M8E_IrqEntry(uint16_t combined_flags)
{
    const uint32_t flags = (uint32_t)combined_flags;
    const uint32_t token = (flags >> 12u) & 3u;
    const uint32_t ep = (flags >> 8u) & 15u;
    const uint32_t transfer = (flags & 2u) != 0u;
    const uint32_t seq = m8e_irq_trace.irq_total + 1u;
    volatile M8E_IrqEvent *record =
        &m8e_irq_trace.events[(seq-1u) & (M8E_IRQ_CAPACITY-1u)];
    const uint32_t ep3 = (uint32_t)UEP_CTRL_LEN(3) |
        ((uint32_t)UEP_CTRL_TX(3) << 8u) |
        ((uint32_t)USBFS->INT_EN << 16u) |
        ((uint32_t)USBFS->UEP2_3_MOD << 24u);
    const uint32_t ctx = (uint32_t)USBFSCTX.endpoints[3].busy |
        ((uint32_t)USBFSCTX.endpoints[3].mode << 8u) |
        ((uint32_t)USBFSCTX.USBFS_SetupReqLen << 16u) |
        ((uint32_t)(USBFSCTX.USBFS_errata_dont_send_endpoint_in_window != 0u) << 31u);
    record->sequence = seq;
    record->main_loops = m8d_usb_trace.main_loops;
    record->combined_flags = flags;
    record->ep3_regs = ep3;
    record->usb_regs = (uint32_t)USBFS->BASE_CTRL |
        ((uint32_t)USBFS->UDEV_CTRL << 8u) |
        ((uint32_t)USBFS->RX_LEN << 16u);
    record->ctx_flags = ctx;
    record->m8d_fg_events = m8d_usb_trace.fg_count;
    record->m8d_irq_callbacks = m8d_usb_trace.irq_count;
    m8e_irq_trace.last_combined = flags;
    m8e_irq_trace.last_ep3 = ep3;
    m8e_irq_trace.last_context = ctx;
    m8e_irq_trace.irq_total = seq;
    if (transfer) {
        m8e_irq_trace.transfer_total++;
        if (token == 2u && ep == 3u) {
            m8e_irq_trace.ep3_in_total++;
            if (flags & (1u<<14)) m8e_irq_trace.ep3_in_tog_ok++;
            else m8e_irq_trace.ep3_in_tog_bad++;
        } else if (token == 0u && ep == 2u) {
            m8e_irq_trace.ep2_out_total++;
        } else if (token == 3u && ep == 0u) {
            m8e_irq_trace.ep0_setup_total++;
        } else {
            m8e_irq_trace.other_total++;
        }
    }
    if (flags & 1u) {
        m8e_irq_trace.bus_reset_total++;
        M8F_OnBusResetIRQ();
    }
    if (flags & 4u) m8e_irq_trace.suspend_total++;
}
