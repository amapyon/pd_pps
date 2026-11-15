#include "pd_hr_recovery.h"

void pd_hr_recovery_init(pd_hr_recovery_t *r)
{
    *r = (pd_hr_recovery_t){0};
}
void pd_hr_recovery_new_session(pd_hr_recovery_t *r)
{
    pd_hr_recovery_init(r);
}
bool pd_hr_recovery_begin(pd_hr_recovery_t *r)
{
    if (r->used) {
        r->active = false;
        r->failed = true;
        return false;
    }
    r->used = true;
    r->active = true;
    return true;
}
pd_hr_recovery_action_t pd_hr_recovery_step(const pd_hr_recovery_t *r,
    const pd_hr_vbus_t *h, bool cc, bool phy_reset, bool tx_timeout)
{
    if (!r->active) return PD_HR_REC_NONE;
    if (!cc || !phy_reset || tx_timeout || h->state == PD_HR_FAILED ||
        h->state == PD_HR_DETACHED || h->state == PD_HR_IDLE)
        return PD_HR_REC_STOP;
    if (!h->low_seen) return PD_HR_REC_NONE;
    /* After three safe-low ADC samples, arm the RX transport BEFORE 5V
       rises. A Source may transmit its first SourceCaps immediately after
       5V reaches tolerance; a 10ms ADC poll must not cause an avoidable
       missed message. Only GoodCRC and staging are allowed in this phase.
       No PE negotiation until three *subsequent* valid 5V samples. */
    if (!r->early_rx_armed &&
        (h->state == PD_HR_WAIT_5V || h->state == PD_HR_5V_OBSERVED))
        return PD_HR_REC_ARM_RX;
    if (r->early_rx_armed && h->state == PD_HR_5V_OBSERVED)
        return PD_HR_REC_COMMIT;
    return PD_HR_REC_NONE;
}
void pd_hr_recovery_armed(pd_hr_recovery_t *r) { r->early_rx_armed = true; }
void pd_hr_recovery_completed(pd_hr_recovery_t *r) {
    r->active = false; r->completed = true;
}
void pd_hr_recovery_failed(pd_hr_recovery_t *r) {
    r->active = false; r->failed = true; r->completed = false;
}
void pd_hr_recovery_detached(pd_hr_recovery_t *r) {
    r->active = false;
    r->early_rx_armed = false;
    r->completed = false;
}

void pd_hr_recovery_gate_ready(pd_hr_recovery_t *r) {
    r->gate_waiting = false;
    r->blocked_gate = 0u;
    r->blocked_since_ms = 0u;
}
bool pd_hr_recovery_gate_busy(pd_hr_recovery_t *r,
                              pd_hr_recovery_action_t action,
                              uint32_t now_ms) {
    if (!r->active || (action != PD_HR_REC_ARM_RX &&
                       action != PD_HR_REC_COMMIT)) return true;
    if (!r->gate_waiting || r->blocked_gate != (uint8_t)action) {
        r->gate_waiting = true;
        r->blocked_gate = (uint8_t)action;
        r->blocked_since_ms = now_ms;
    }
    /* 32-bit unsigned delta, never compare absolute deadlines across wrap. */
    return (uint32_t)(now_ms - r->blocked_since_ms) >=
           PD_HR_REC_GATE_TIMEOUT_MS;
}
