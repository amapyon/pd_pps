#include "m4_apply_guard.h"

/* Pilot policy: reject power-limited PPS APDOs until separately tested;
 * do not permit very small current requests that could brown out the board.
 * These are guards on the PD REQUEST, not a load current limiter. */
m4_apply_verdict_t m4_apply_validate(const m4_apply_check_t *check)
{
    if (!check || !check->pd_ready) return M4_APPLY_PD_NOT_READY;
    if (!check->original_target_unchanged) return M4_APPLY_TARGET_CHANGED;
    if (!check->source_apdo_unchanged) return M4_APPLY_SOURCE_CHANGED;
    if (check->source_power_limited) return M4_APPLY_POWER_LIMITED;
    if (!pps_apdo_accepts(check->apdo, check->mv, check->ma) ||
        check->mv < 3300u || check->mv > 21000u)
        return M4_APPLY_OUT_OF_RANGE;
    if (check->ma < 200u) return M4_APPLY_LOW_REQUEST;
    return M4_APPLY_OK;
}

const char *m4_apply_message(m4_apply_verdict_t verdict)
{
    switch (verdict) {
        case M4_APPLY_OK:               return "PPS APPLIED";
        case M4_APPLY_PD_NOT_READY:     return "PD NOT READY";
        case M4_APPLY_TARGET_CHANGED:   return "TARGET CHANGED";
        case M4_APPLY_SOURCE_CHANGED:   return "SOURCE CAPS CHANGED";
        case M4_APPLY_POWER_LIMITED:    return "APDO POWER LIMITED";
        case M4_APPLY_OUT_OF_RANGE:     return "APDO REJECTED";
        case M4_APPLY_LOW_REQUEST:      return "REQ BELOW 200MA";
        default:                        return "APPLY REJECTED";
    }
}
