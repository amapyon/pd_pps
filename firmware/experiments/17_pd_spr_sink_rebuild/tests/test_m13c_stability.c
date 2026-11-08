#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pd_stability.h"

static void test_wrap_and_aggregate(void) {
    pd_stability_t s;
    pd_stability_init(&s);
    pd_stability_attach(&s, UINT32_MAX - 20u, 125u, 9u);
    assert(s.active && s.pps_ready == 0 && s.last_ms == UINT32_MAX - 20u);
    pd_stability_tick(&s, UINT32_MAX - 10u, true);
    pd_stability_tick(&s, 30u, true); /* wrapped 32-bit millisecond clock */
    assert(s.elapsed_ms == 51u && s.ready_ms == 41u && s.longest_loop_ms == 41u);
    pe_t p = {0};
    p.state = PE_WAIT_ACCEPT;
    pd_stability_event(&s, PE_EV_CAPS, PE_WAIT_CAPS, &p);
    pd_stability_command_sent(&s, PE_CMD_REQUEST);
    pd_stability_command_sent(&s, PE_CMD_GET_SOURCE_CAP);
    assert(s.requests_issued == 1u && s.caps_received == 1u);
    p.state = PE_READY; p.contract_valid = true; p.pps_active = true;
    pd_stability_event(&s, PE_EV_PS_RDY, PE_WAIT_PS_RDY, &p);
    pd_stability_refresh_started(&s);
    pd_stability_command_sent(&s, PE_CMD_REQUEST);
    pd_stability_event(&s, PE_EV_PS_RDY, PE_WAIT_PS_RDY, &p);
    assert(s.pps_ready == 2u && s.refresh_started == 1u && s.refresh_completed == 1u);
    p.pps_active = false;
    pd_stability_event(&s, PE_EV_PS_RDY, PE_WAIT_PS_RDY, &p);
    assert(s.fixed_ready == 1u);
    pd_stability_event(&s, PE_EV_HARD_RESET_RX, PE_READY, &p);
    p.state = PE_RECOVERY; p.contract_valid = false;
    pd_stability_event(&s, PE_EV_SENSOR_FAULT, PE_READY, &p);
    assert(s.source_hr_rx == 1u && s.adc_fault == 1u && s.recovery_entries == 1u);
    pd_stability_detach(&s, 80u, 139u, 11u, 2u);
    assert(!s.active && s.elapsed_ms == 101u && s.ready_ms == 91u);
    assert(s.trace_drops_delta == 14u && s.lost_delta == 2u && s.faults_at_end == 2u);
    pd_stability_tick(&s, 300u, true);
    assert(s.elapsed_ms == 101u); /* frozen on detach */
}
static void test_new_session_and_saturation(void) {
    pd_stability_t s;
    pd_stability_attach(&s, 400u, 42u, 0u);
    s.caps_received = UINT32_MAX;
    pe_t p = {0}; p.state = PE_WAIT_CAPS;
    pd_stability_event(&s, PE_EV_CAPS, PE_WAIT_CAPS, &p);
    assert(s.caps_received == UINT32_MAX);
    s.ready_ms = UINT32_MAX - 1u;
    s.last_ready = true;
    pd_stability_tick(&s, 410u, true);
    assert(s.ready_ms == UINT32_MAX);
    pd_stability_attach(&s, 1000u, 77u, 3u);
    assert(s.ready_ms == 0u && s.caps_received == 0u && s.active);
}
int main(void) {
    test_wrap_and_aggregate();
    test_new_session_and_saturation();
    puts("M13c long-run stats wrap, saturation, session reset: PASS");
    return 0;
}
