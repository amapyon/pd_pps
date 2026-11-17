#include "pd_policy.h"
#include "pd_power.h"
#include <string.h>
/* Trial policy: only 7000 mV / 1000 mA; never assume an APDO persists. */
#define PE_MV 7000u
#define PE_MA 1000u
#define PE_ACCEPT_TIMEOUT_MS 45u
#define PE_PS_RDY_TIMEOUT_MS 500u
#define PE_CAPS_TIMEOUT_MS 650u
#define PE_PPS_REFRESH_MS 5000u
#define PE_WAIT_RETRY_MS 120u
/* Bench sink: do not continue indefinitely when a Source refuses PPS renewal.
   At most two retry Requests after the first WAIT (three WAITs total). */
#define PE_MAX_PPS_WAIT_REPLIES 3u

static uint32_t elapsed(uint32_t now, uint32_t since) { return now - since; }
static bool reached(uint32_t now, uint32_t deadline) { return (int32_t)(now - deadline) >= 0; }
static void invalidate(pe_t *p) {
    p->contract_valid = false; p->pps_active = false; p->pending_pps = false;
    p->request_sent = false; p->command = PE_CMD_NONE; p->caps_fresh = false;
    p->defer_active = false; p->pps_wait_count = 0u;
}
void pe_init(pe_t *p) { memset(p, 0, sizeof(*p)); p->state = PE_DETACHED; }
/* Common contract-dependent cleanup for both fault and Hard Reset recovery.
 * The caller retains the original state guards and distinct HR flag. */
static void enter_recovery_after_power_loss(pe_t *p, bool awaiting_hr)
{
    invalidate(p);
    p->pending_soft_reset = false;
    p->caps = (pe_caps_t){0};
    p->caps_epoch = 0u;
    p->pdo_index = 0u;
    p->object_pos = 0u;
    p->rdo = 0u;
    p->requested_mv = 0u;
    p->requested_ma = 0u;
    p->last_pps_ms = 0u;
    p->deadline_ms = 0u;
    p->defer_until_ms = 0u;
    p->state = PE_RECOVERY;
    p->awaiting_hr_recovery = awaiting_hr;
    p->faults++;
}

static void select_request(pe_t *p, uint32_t now) {
    /* A pure selection step, retaining the original 7 V / 1 A preference
     * and fixed 5 V fallback. PE owns all state transitions and timers. */
    pd_power_plan_t plan;
    pd_power_result_t result = pd_power_select(p->caps.pdo, p->caps.count,
                                               PE_MV, PE_MA, &plan);
    if (result != PD_POWER_PPS && result != PD_POWER_FIXED_5V) {
        p->state = PE_RECOVERY; p->faults++; invalidate(p); return;
    }
    p->object_pos = plan.object_pos;
    p->rdo = plan.rdo;
    p->pending_pps = plan.pps;
    p->command = PE_CMD_REQUEST;
    p->request_sent = false;
    p->state = PE_WAIT_ACCEPT;
    p->deadline_ms = now + PE_ACCEPT_TIMEOUT_MS;
}
void pe_event(pe_t *p, pe_event_t ev, const pe_caps_t *caps, uint32_t now) {
    if (ev == PE_EV_DETACH) { pe_init(p); return; }
    if (ev == PE_EV_VBUS_LOST || ev == PE_EV_SENSOR_FAULT) {
        /* Measured VBUS loss OR failed ADC monitoring invalidates the contract
           immediately. Neither event is a Hard Reset. Stop until real
           CC detach, and never treat voltage return as negotiation success. */
        if (p->state != PE_DETACHED && p->state != PE_RECOVERY) {
            enter_recovery_after_power_loss(p, false);
        }
        return;
    }
    if (ev == PE_EV_HARD_RESET_RX || ev == PE_EV_HARD_RESET_TX) {
        /* The explicit contract is lost immediately on Hard Reset.
           Clear all contract-dependent data and pending AMS work.
           PE_RECOVERY has NO timer-based exit: the M13b coordinator alone
           may leave recovery after conservative low -> 5V observation and
           atomic PHY protocol reset. Otherwise a genuine physical CC
           detach is required. */
        enter_recovery_after_power_loss(p, true);
        return;
    }
    /* Only the M13a-6 coordinator may issue this event after a measured
       low -> 5V transition and an atomic PHY protocol-layer reset. */
    if (ev == PE_EV_HR_VBUS_READY) {
        if (p->state == PE_RECOVERY && p->awaiting_hr_recovery &&
            !p->contract_valid && p->command == PE_CMD_NONE) {
            p->awaiting_hr_recovery = false;
            p->state = PE_WAIT_CAPS;
            p->deadline_ms = now + PE_CAPS_TIMEOUT_MS;
        }
        return;
    }
    if (ev == PE_EV_ATTACH) {
        if (p->state == PE_DETACHED) {
            invalidate(p); p->state = PE_WAIT_CAPS; p->deadline_ms = now + PE_CAPS_TIMEOUT_MS;
        }
        return;
    }
    if (p->state == PE_DETACHED || p->state == PE_RECOVERY) return;
    if (ev == PE_EV_SOFT_RESET_RX) {
        /* A Soft Reset reinitializes protocol message counters in PHY.
           It does not itself invalidate the explicit power contract. */
        p->pending_soft_reset = true; p->command = PE_CMD_ACCEPT_SOFT_RESET;
        p->defer_active = false; p->pps_wait_count = 0u;
        p->request_sent = false;
        p->state = PE_WAIT_CAPS; p->deadline_ms = now + PE_CAPS_TIMEOUT_MS;
        return;
    }
    /* M12d: incoming SOP messages must not start an overlapping AMS.
       A fresh Source_Capabilities is allowed in READY or while waiting for
       capabilities, or can replace a Request that has NOT yet been sent.
       After Request has started, during power transition, or before our
       Soft_Reset Accept is sent, accepting CAPS would overwrite the current
       command/transition. Until M13 implements Soft/Hard Reset error recovery,
       fail closed rather than issue an unsafe second Request. */
    if (ev == PE_EV_CAPS &&
        (p->state == PE_WAIT_PS_RDY ||
         p->state == PE_SOFT_RESET_WAIT ||
         (p->state == PE_WAIT_ACCEPT && p->request_sent) ||
         p->command == PE_CMD_ACCEPT_SOFT_RESET)) {
        invalidate(p);
        p->state = PE_RECOVERY;
        p->faults++;
        return;
    }
    if (ev == PE_EV_CAPS && caps && caps->count > 0 && caps->count <= 7) {
        /* A new Source_Capabilities starts a new power negotiation AMS.
           A true transport duplicate must have been filtered in protocol. */
        p->caps = *caps; p->caps_fresh = true; p->caps_epoch++;
        select_request(p, now); return;
    }
    if (ev == PE_EV_TX_FAILED) { p->state = PE_RECOVERY; invalidate(p); p->faults++; return; }
    if (p->state == PE_SOFT_RESET_WAIT) {
        if (ev == PE_EV_ACCEPT) { p->state = PE_WAIT_CAPS; p->deadline_ms = now + PE_CAPS_TIMEOUT_MS; }
        else if (ev == PE_EV_REJECT || ev == PE_EV_WAIT) { p->state = PE_RECOVERY; invalidate(p); p->faults++; }
        return;
    }
    if (p->state == PE_WAIT_ACCEPT) {
        if (ev == PE_EV_ACCEPT && p->request_sent) {
            p->state = PE_WAIT_PS_RDY;
            p->deadline_ms = now + PE_PS_RDY_TIMEOUT_MS;
        } else if (ev == PE_EV_REJECT || ev == PE_EV_WAIT) {
            /* A response before our Request was actually transmitted cannot
               belong to this AMS. Never leave a stale Request queued. */
            if (!p->request_sent ||
                /* A REJECT of a renewal must not cause a per-loop flood of
                   Request messages after the 5 s renewal deadline. */
                (p->contract_valid && ev == PE_EV_REJECT) ||
                /* Bound consecutive WAITs. With an existing PPS contract,
                   indefinitely retrying every 120 ms is not safe. */
                (p->contract_valid && p->pps_active && ev == PE_EV_WAIT &&
                 p->pps_wait_count + 1u >= PE_MAX_PPS_WAIT_REPLIES)) {
                p->state = PE_RECOVERY;
                invalidate(p);
                p->faults++;
                return;
            }
            if (p->contract_valid) {
                p->state = PE_READY;
                p->pending_pps = p->pps_active;
            } else {
                p->state = PE_WAIT_CAPS;
                p->deadline_ms = now + PE_CAPS_TIMEOUT_MS;
            }
            p->request_sent = false;
            if (ev == PE_EV_WAIT) {
                if (p->contract_valid && p->pps_active) p->pps_wait_count++;
                p->defer_until_ms = now + PE_WAIT_RETRY_MS;
                p->defer_active = true;
            }
        }
        return;
    }
    if (p->state == PE_WAIT_PS_RDY && ev == PE_EV_PS_RDY) {
        p->state = PE_READY; p->contract_valid = true;
        p->pps_active = p->pending_pps;
        p->requested_mv = p->pending_pps ? PE_MV : 5000u;
        p->requested_ma = PE_MA;
        p->last_pps_ms = now;
        p->pps_wait_count = 0u;
        p->defer_active = false;
    }
}
void pe_command_result(pe_t *p, bool accepted, uint32_t now) {
    if (!accepted) return; /* Controller is busy; retry on next tick. */
    if (p->command == PE_CMD_REQUEST) { p->request_sent = true; p->deadline_ms = now + PE_ACCEPT_TIMEOUT_MS; }
    if (p->command == PE_CMD_SOFT_RESET) { p->state = PE_SOFT_RESET_WAIT; p->deadline_ms = now + PE_ACCEPT_TIMEOUT_MS; }
    if (p->command == PE_CMD_ACCEPT_SOFT_RESET) { p->state = PE_WAIT_CAPS; p->deadline_ms = now + PE_CAPS_TIMEOUT_MS; }
    p->command = PE_CMD_NONE;
}
void pe_tick(pe_t *p, uint32_t now) {
    /* A zero deadline is not an "inactive" timestamp: once 32-bit uptime
       passes 0x80000000, reached(now, 0) becomes false for ~24 days.
       Track backoff validity separately and retire expired deadlines.
       Subtraction remains wrap-safe for the short (120ms) WAIT interval. */
    if (p->defer_active && reached(now, p->defer_until_ms))
        p->defer_active = false;
    if (p->state == PE_WAIT_CAPS && reached(now, p->deadline_ms)) {
        p->state = PE_RECOVERY; invalidate(p); p->faults++;
    }
    /* Missing Request start must not silently stall forever. */
    if (p->state == PE_WAIT_ACCEPT && !p->request_sent &&
        reached(now, p->deadline_ms - (PE_ACCEPT_TIMEOUT_MS - 15u))) {
        p->state = PE_RECOVERY; invalidate(p); p->faults++;
    }
    if ((p->state == PE_WAIT_ACCEPT && p->request_sent && reached(now, p->deadline_ms)) ||
        (p->state == PE_WAIT_PS_RDY && reached(now, p->deadline_ms)) ||
        (p->state == PE_SOFT_RESET_WAIT && reached(now, p->deadline_ms))) {
        p->state = PE_RECOVERY; invalidate(p); p->faults++;
    }
    if (p->state == PE_READY && p->pps_active && p->command == PE_CMD_NONE &&
        !p->defer_active && elapsed(now, p->last_pps_ms) >= PE_PPS_REFRESH_MS) {
        /* Re-send the same PPS request; not a synthetic Source_Capabilities. */
        select_request(p, now);
    }
}
bool pe_request_soft_reset(pe_t *p, uint32_t now) {
    if (p->state != PE_READY || p->command != PE_CMD_NONE) return false;
    p->command = PE_CMD_SOFT_RESET; p->deadline_ms = now + PE_ACCEPT_TIMEOUT_MS;
    return true;
}

/* M11b bench-only: no PE state change until a real Source_Capabilities arrives.
   Uses the normal command path and leaves PPS refresh/contract intact. */
bool pe_request_source_caps(pe_t *p) {
    if (p->state != PE_READY || !p->contract_valid || p->command != PE_CMD_NONE) return false;
    p->command = PE_CMD_GET_SOURCE_CAP;
    return true;
}
