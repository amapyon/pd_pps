#include "pd_stability.h"
#include <string.h>

/* Saturate long-running counters rather than wrap into misleading zeroes. */
static void inc(uint32_t *v) { if (*v != UINT32_MAX) ++*v; }
static void add(uint32_t *v, uint32_t d) {
    if (UINT32_MAX - *v < d) *v = UINT32_MAX;
    else *v += d;
}
void pd_stability_init(pd_stability_t *s) { memset(s, 0, sizeof(*s)); }
void pd_stability_attach(pd_stability_t *s, uint32_t now_ms,
                         uint32_t trace_drops, uint32_t event_lost) {
    pd_stability_init(s);
    s->started_ms = now_ms;
    s->last_ms = now_ms;
    s->trace_drops_at_start = trace_drops;
    s->lost_at_start = event_lost;
    s->active = true;
}
void pd_stability_tick(pd_stability_t *s, uint32_t now_ms,
                       bool is_ready_with_contract) {
    if (!s->active) return;
    uint32_t dt = now_ms - s->last_ms; /* unsigned, 32-bit wrap safe */
    if (dt > s->longest_loop_ms) s->longest_loop_ms = dt;
    if (s->last_ready) add(&s->ready_ms, dt);
    s->last_ready = is_ready_with_contract;
    s->last_ms = now_ms;
    s->elapsed_ms = now_ms - s->started_ms;
}
void pd_stability_event(pd_stability_t *s, pe_event_t ev,
                        pe_state_t before, const pe_t *after) {
    if (!s->active) return;
    if (ev == PE_EV_CAPS) inc(&s->caps_received);
    if (ev == PE_EV_HARD_RESET_RX) inc(&s->source_hr_rx);
    if (ev == PE_EV_VBUS_LOST) inc(&s->vbus_loss);
    if (ev == PE_EV_SENSOR_FAULT) inc(&s->adc_fault);
    if (before != PE_RECOVERY && after->state == PE_RECOVERY) {
        inc(&s->recovery_entries);
        s->refresh_in_flight = false;
    }
    if (ev == PE_EV_PS_RDY && before == PE_WAIT_PS_RDY &&
        after->state == PE_READY && after->contract_valid) {
        if (after->pps_active) {
            inc(&s->pps_ready);
            if (s->refresh_in_flight) inc(&s->refresh_completed);
        } else inc(&s->fixed_ready);
        s->refresh_in_flight = false;
    }
}
void pd_stability_refresh_started(pd_stability_t *s) {
    if (!s->active) return;
    inc(&s->refresh_started);
    s->refresh_in_flight = true;
}
void pd_stability_command_sent(pd_stability_t *s, pe_cmd_t cmd) {
    if (s->active && cmd == PE_CMD_REQUEST) inc(&s->requests_issued);
}
void pd_stability_detach(pd_stability_t *s, uint32_t now_ms,
                         uint32_t trace_drops, uint32_t event_lost,
                         uint32_t policy_faults) {
    if (!s->active) return;
    pd_stability_tick(s, now_ms, false);
    s->active = false;
    s->faults_at_end = policy_faults;
    s->trace_drops_delta = trace_drops - s->trace_drops_at_start;
    s->lost_delta = event_lost - s->lost_at_start;
}
