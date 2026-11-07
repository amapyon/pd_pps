#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pd_stop_diag.h"

static void test_first_stop_and_detach(void) {
    pd_stop_diag_t d;
    pd_stop_diag_init(&d);
    assert(!d.active && !d.found && d.reason == PD_STOP_NONE);
    pd_stop_diag_note(&d, PD_STOP_ADC_MONITOR, 100, PE_READY, PE_RECOVERY, 1);
    assert(!d.found);
    pd_stop_diag_attach(&d, UINT32_MAX - 20u);
    pd_stop_diag_rx_hr(&d);
    pd_stop_diag_note(&d, PD_STOP_PE_TIMEOUT_ACCEPT, 19u,
                      PE_WAIT_ACCEPT, PE_RECOVERY, 0u);
    assert(d.found && d.reason == PD_STOP_PE_TIMEOUT_ACCEPT);
    assert(d.stop_rel_ms == 40u);
    pd_stop_diag_note(&d, PD_STOP_VBUS_LOSS, 20u,
                      PE_READY, PE_RECOVERY, 18u);
    assert(d.reason == PD_STOP_PE_TIMEOUT_ACCEPT && d.aux == 0u);
    pd_stop_diag_detach(&d, 139u);
    assert(d.detach_seen && d.detach_after_stop_ms == 120u);
    assert(!d.active && d.hr_rx_count == 1u);
    pd_stop_diag_rx_hr(&d);
    assert(d.hr_rx_count == 1u);
    pd_stop_diag_attach(&d, 1000u);
    assert(!d.found && !d.detach_seen && !d.hr_rx_count && !d.reason);
    pd_stop_diag_rx_hr(&d);
    pd_stop_diag_rx_hr(&d);
    assert(d.hr_rx_count == 2u);
}
static void test_reason_tables(void) {
    assert(pd_stop_diag_pe_tick_reason(PE_WAIT_CAPS) == PD_STOP_PE_TIMEOUT_CAPS);
    assert(pd_stop_diag_pe_tick_reason(PE_WAIT_ACCEPT) == PD_STOP_PE_TIMEOUT_ACCEPT);
    assert(pd_stop_diag_pe_tick_reason(PE_WAIT_PS_RDY) == PD_STOP_PE_TIMEOUT_PS_RDY);
    assert(pd_stop_diag_pe_tick_reason(PE_SOFT_RESET_WAIT) == PD_STOP_PE_TIMEOUT_SOFT_RESET);
    assert(pd_stop_diag_pe_tick_reason(PE_READY) == PD_STOP_PE_OTHER);
    assert(pd_stop_diag_pe_event_reason(PE_EV_TX_FAILED) == PD_STOP_PE_TX_FAILED);
    assert(pd_stop_diag_pe_event_reason(PE_EV_CAPS) == PD_STOP_PE_CAPS);
    assert(pd_stop_diag_pe_event_reason(PE_EV_REJECT) == PD_STOP_PE_OTHER);
}
static void test_never_mutate_after_first(void) {
    pd_stop_diag_t d;
    pd_stop_diag_attach(&d, 900u);
    pd_stop_diag_note(&d, PD_STOP_NONE, 905u, PE_READY, PE_READY, 0);
    assert(!d.found);
    pd_stop_diag_note(&d, PD_STOP_ADC_MONITOR, 910u,
                      PE_READY, PE_RECOVERY, 1u);
    assert(d.pe_before == PE_READY && d.pe_after == PE_RECOVERY);
    pd_stop_diag_detach(&d, 1010u);
    pd_stop_diag_note(&d, PD_STOP_PE_CAPS, 1020u,
                      PE_WAIT_CAPS, PE_RECOVERY, 2u);
    assert(d.reason == PD_STOP_ADC_MONITOR && d.detach_after_stop_ms == 100u);
}
int main(void) {
    test_first_stop_and_detach();
    test_reason_tables();
    test_never_mutate_after_first();
    puts("M13c-2 stop reason, first failure, CC wrap, session reset: PASS");
    return 0;
}
