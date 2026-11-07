#include "pd_hr_pps_probe.h"

static bool ready(const pe_t *pe)
{
    return pe->state == PE_READY && pe->contract_valid &&
           pe->command == PE_CMD_NONE;
}
static bool elapsed(uint32_t now, uint32_t since, uint32_t duration)
{
    return (uint32_t)(now - since) >= duration;
}
void pd_hr_pps_probe_init(pd_hr_pps_probe_t *p)
{
    *p = (pd_hr_pps_probe_t){0};
}
bool pd_hr_pps_probe_step(pd_hr_pps_probe_t *p, uint32_t now,
                          bool recovery_complete, bool attached,
                          bool phy_blocked, const pe_t *pe)
{
    if (p->state == PD_HR_PPS_PROBE_IDLE && recovery_complete &&
        attached && !phy_blocked && ready(pe)) {
        if (pe->pps_active) {
            p->state = PD_HR_PPS_PROBE_PPS_ALREADY_READY;
        } else if (pe->requested_mv == 5000u &&
                   pe->requested_ma == 1000u) {
            p->ready_start_ms = now;
            p->ready_timer_valid = true;
            p->state = PD_HR_PPS_PROBE_DELAY;
        }
    }
    /* Do not count power-transition time toward stable fixed-5V Ready.
       Other/new SourceCaps AMS activity restarts the 1-second interval. */
    if (p->state == PD_HR_PPS_PROBE_DELAY) {
        if (!attached || phy_blocked || pe->state == PE_RECOVERY ||
            !recovery_complete) {
            p->state = PD_HR_PPS_PROBE_ABORTED;
        } else if (ready(pe) && pe->pps_active) {
            p->state = PD_HR_PPS_PROBE_PPS_ALREADY_READY;
        } else if (!ready(pe)) {
            p->ready_timer_valid = false;
        } else if (!p->ready_timer_valid) {
            p->ready_start_ms = now;
            p->ready_timer_valid = true;
        } else if (elapsed(now, p->ready_start_ms,
                           PD_HR_PPS_PROBE_READY_DELAY_MS)) {
            return true;
        }
    } else if (p->state == PD_HR_PPS_PROBE_TX_QUEUED) {
        if (!attached || phy_blocked || pe->state == PE_RECOVERY) {
            p->state = PD_HR_PPS_PROBE_ABORTED;
        } else if (elapsed(now, p->request_start_ms,
                           PD_HR_PPS_PROBE_TX_START_TIMEOUT_MS)) {
            /* Caller may cancel an UNSENT GET_SOURCE_CAP command; never
               cancel a command after phy_send() has accepted it. */
            p->timeout = true;
            p->state = PD_HR_PPS_PROBE_NO_RESPONSE;
        }
    } else if (p->state == PD_HR_PPS_PROBE_WAIT_CAPS) {
        if (!attached || phy_blocked || pe->state == PE_RECOVERY) {
            p->state = PD_HR_PPS_PROBE_ABORTED;
        } else if (elapsed(now, p->request_start_ms,
                           PD_HR_PPS_PROBE_RESPONSE_TIMEOUT_MS)) {
            /* A missing SourceCaps response does not revoke an existing
               explicit fixed-5V contract. Never poll a second time. */
            p->timeout = true;
            p->state = PD_HR_PPS_PROBE_NO_RESPONSE;
        }
    } else if (p->state == PD_HR_PPS_PROBE_PPS_OFFERED) {
        if (!attached || phy_blocked || pe->state == PE_RECOVERY) {
            p->state = PD_HR_PPS_PROBE_ABORTED;
        } else if (ready(pe) && pe->pps_active) {
            p->pps_ready = true;
            p->state = PD_HR_PPS_PROBE_PPS_READY;
        }
    }
    return false;
}
void pd_hr_pps_probe_queued(pd_hr_pps_probe_t *p, uint32_t now)
{
    if (p->state == PD_HR_PPS_PROBE_DELAY) {
        p->request_start_ms = now;
        p->state = PD_HR_PPS_PROBE_TX_QUEUED;
    }
}
void pd_hr_pps_probe_tx_started(pd_hr_pps_probe_t *p, uint32_t now)
{
    if (p->state == PD_HR_PPS_PROBE_TX_QUEUED) {
        p->sent = true;
        p->request_start_ms = now;
        p->state = PD_HR_PPS_PROBE_WAIT_CAPS;
    }
}
void pd_hr_pps_probe_caps(pd_hr_pps_probe_t *p, const pe_t *pe)
{
    if (p->state != PD_HR_PPS_PROBE_WAIT_CAPS) return;
    p->caps_seen = true;
    if (pe->state != PE_WAIT_ACCEPT || pe->command != PE_CMD_REQUEST) {
        /* e.g. invalid SourceCaps caused the PE to fail closed. */
        p->state = PD_HR_PPS_PROBE_ABORTED;
        return;
    }
    p->pps_offered = pe->pending_pps;
    p->state = pe->pending_pps ? PD_HR_PPS_PROBE_PPS_OFFERED :
                                 PD_HR_PPS_PROBE_FIXED_ONLY;
}
