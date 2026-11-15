#ifndef PD_STABILITY_H
#define PD_STABILITY_H
#include <stdbool.h>
#include <stdint.h>
#include "pd_policy.h"

/* M13c: fixed-size session metrics independent of the 127-entry PHY trace.
 * Strictly observational: no I/O, allocations or hardware calls. All public
 * functions run in the foreground, never in USBPD_IRQHandler. */
typedef struct {
    uint32_t started_ms, last_ms, elapsed_ms, ready_ms, longest_loop_ms;
    uint32_t caps_received, requests_issued, pps_ready, fixed_ready;
    uint32_t refresh_started, refresh_completed;
    uint32_t source_hr_rx, recovery_entries, vbus_loss, adc_fault;
    uint32_t trace_drops_at_start, trace_drops_delta;
    uint32_t lost_at_start, lost_delta;
    uint32_t faults_at_end;
    bool active, last_ready, refresh_in_flight;
} pd_stability_t;

void pd_stability_init(pd_stability_t *s);
void pd_stability_attach(pd_stability_t *s, uint32_t now_ms,
                         uint32_t trace_drops, uint32_t event_lost);
void pd_stability_tick(pd_stability_t *s, uint32_t now_ms,
                       bool is_ready_with_contract);
void pd_stability_event(pd_stability_t *s, pe_event_t ev,
                        pe_state_t before, const pe_t *after);
void pd_stability_refresh_started(pd_stability_t *s);
void pd_stability_command_sent(pd_stability_t *s, pe_cmd_t cmd);
void pd_stability_detach(pd_stability_t *s, uint32_t now_ms,
                         uint32_t trace_drops, uint32_t event_lost,
                         uint32_t policy_faults);
#endif
