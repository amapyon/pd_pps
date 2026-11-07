#include "pd_policy.h"
#include <string.h>
/* Trial policy: only 7000 mV / 1000 mA; never assume an APDO persists. */
#define PE_MV 7000u
#define PE_MA 1000u
#define PE_ACCEPT_TIMEOUT_MS 45u
#define PE_PS_RDY_TIMEOUT_MS 500u
#define PE_CAPS_TIMEOUT_MS 650u
#define PE_PPS_REFRESH_MS 5000u
#define PE_WAIT_RETRY_MS 120u

static uint32_t elapsed(uint32_t now, uint32_t since) { return now - since; }
static bool reached(uint32_t now, uint32_t deadline) { return (int32_t)(now - deadline) >= 0; }
static void invalidate(pe_t *p) {
    p->contract_valid = false; p->pps_active = false; p->pending_pps = false;
    p->request_sent = false; p->command = PE_CMD_NONE; p->caps_fresh = false;
    p->defer_active = false;
}
void pe_init(pe_t *p) { memset(p, 0, sizeof(*p)); p->state = PE_DETACHED; }
static bool choose_pps(pe_t *p) {
    for (uint8_t i = 0; i < p->caps.count; i++) {
        uint32_t v = p->caps.pdo[i];
        if ((v >> 30) != 3u || ((v >> 28) & 3u) != 0u) continue;
        uint32_t min_mv = ((v >> 8) & 0xffu) * 100u;
        uint32_t max_mv = ((v >> 17) & 0xffu) * 100u;
        uint32_t max_ma = (v & 0x7fu) * 50u;
        if (PE_MV < min_mv || PE_MV > max_mv || PE_MA > max_ma) continue;
        p->object_pos = (uint8_t)(i + 1u);
        p->rdo = ((uint32_t)p->object_pos << 28) |
                 (1u << 25) | (1u << 24) | ((PE_MV / 20u) << 9) | (PE_MA / 50u);
        p->pending_pps = true;
        return true;
    }
    return false;
}
static bool choose_fixed5(pe_t *p) {
    if (p->caps.count == 0) return false;
    uint32_t v = p->caps.pdo[0];
    uint32_t mv = ((v >> 10) & 0x3ffu) * 50u;
    uint32_t ma = (v & 0x3ffu) * 10u;
    if ((v >> 30) != 0 || mv != 5000u || ma < 1000u) return false;
    p->object_pos = 1;
    p->rdo = (1u << 28) | (1u << 25) | (1u << 24) | (100u << 10) | 100u;
    p->pending_pps = false;
    return true;
}
static void select_request(pe_t *p, uint32_t now) {
    /* A Source_Capabilities message starts with a valid fixed 5 V PDO. */
    const uint32_t first = p->caps.pdo[0];
    if (p->caps.count == 0u || (first >> 30) != 0u ||
        ((first >> 10) & 0x3ffu) != 100u) {
        p->state = PE_RECOVERY; p->faults++; invalidate(p); return;
    }
    /* Every new (non-duplicate) Source_Capabilities requires evaluation,
       including unchanged data offered with a new MessageID in READY. */
    if (!choose_pps(p) && !choose_fixed5(p)) { p->state = PE_RECOVERY; p->faults++; invalidate(p); return; }
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
            p->awaiting_hr_recovery = false;
            p->faults++;
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
        invalidate(p);
        p->pending_soft_reset = false;
        p->caps = (pe_caps_t){0};
        p->caps_epoch = 0;
        p->pdo_index = 0;
        p->object_pos = 0;
        p->rdo = 0;
        p->requested_mv = 0;
        p->requested_ma = 0;
        p->last_pps_ms = 0;
        p->deadline_ms = 0;
        p->defer_until_ms = 0;
        p->state = PE_RECOVERY;
        p->awaiting_hr_recovery = true;
        p->faults++;
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
        if (ev == PE_EV_ACCEPT && p->request_sent) { p->state = PE_WAIT_PS_RDY; p->deadline_ms = now + PE_PS_RDY_TIMEOUT_MS; }
        else if (ev == PE_EV_REJECT || ev == PE_EV_WAIT) {
            if (p->contract_valid) { p->state = PE_READY; p->pending_pps = p->pps_active; }
            else { p->state = PE_WAIT_CAPS; p->deadline_ms = now + PE_CAPS_TIMEOUT_MS; }
            if (ev == PE_EV_WAIT) {
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
