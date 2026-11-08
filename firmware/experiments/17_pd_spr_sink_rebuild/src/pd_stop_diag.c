#include "pd_stop_diag.h"
#include <string.h>

void pd_stop_diag_init(pd_stop_diag_t *d) { memset(d, 0, sizeof(*d)); }
void pd_stop_diag_attach(pd_stop_diag_t *d, uint32_t now_ms) {
    pd_stop_diag_init(d);
    d->active = true;
    d->started_ms = now_ms;
}
void pd_stop_diag_rx_hr(pd_stop_diag_t *d) {
    if (d->active && d->hr_rx_count < UINT32_MAX) d->hr_rx_count++;
}
void pd_stop_diag_note(pd_stop_diag_t *d, pd_stop_reason_t why,
                       uint32_t now_ms, pe_state_t before,
                       pe_state_t after, uint16_t aux) {
    /* Preserve the first stop even if detachment, further IRQs or a later
       recovery failure follows. Clock subtraction handles 32-bit wrap. */
    if (!d->active || d->found || why == PD_STOP_NONE) return;
    d->found = true;
    d->reason = (uint8_t)why;
    d->pe_before = (uint8_t)before;
    d->pe_after = (uint8_t)after;
    d->stop_rel_ms = now_ms - d->started_ms;
    d->aux = aux;
}
void pd_stop_diag_detach(pd_stop_diag_t *d, uint32_t now_ms) {
    if (!d->active) return;
    d->detach_seen = true;
    d->active = false;
    if (d->found) {
        uint32_t elapsed_ms = now_ms - d->started_ms;
        d->detach_after_stop_ms = elapsed_ms - d->stop_rel_ms;
    }
}
pd_stop_reason_t pd_stop_diag_pe_tick_reason(pe_state_t before) {
    switch (before) {
    case PE_WAIT_CAPS: return PD_STOP_PE_TIMEOUT_CAPS;
    case PE_WAIT_ACCEPT: return PD_STOP_PE_TIMEOUT_ACCEPT;
    case PE_WAIT_PS_RDY: return PD_STOP_PE_TIMEOUT_PS_RDY;
    case PE_SOFT_RESET_WAIT: return PD_STOP_PE_TIMEOUT_SOFT_RESET;
    default: return PD_STOP_PE_OTHER;
    }
}
pd_stop_reason_t pd_stop_diag_pe_event_reason(pe_event_t event) {
    switch (event) {
    case PE_EV_CAPS: return PD_STOP_PE_CAPS;
    case PE_EV_TX_FAILED: return PD_STOP_PE_TX_FAILED;
    default: return PD_STOP_PE_OTHER;
    }
}
