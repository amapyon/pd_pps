#include "pd_power.h"

/* Keep the original RDO flags (bit 25 and 24) and fixed-5V fallback. */
#define PD_POWER_RDO_FLAGS ((1u << 25) | (1u << 24))
#define PD_POWER_FIXED_5V_RDO ((1u << 28) | PD_POWER_RDO_FLAGS | \
                               (100u << 10) | 100u)

pd_power_result_t pd_power_select(const uint32_t *pdo, uint8_t count,
                                  uint32_t target_mv, uint32_t target_ma,
                                  pd_power_plan_t *out)
{
    if (!pdo || !out || count == 0u || count > PD_POWER_MAX_PDOS)
        return PD_POWER_INVALID_CAPS;

    /* Retain M43B-7's prerequisite: PDO 1 must be fixed 5 V.
     * The original policy does NOT require >=1 A here if PPS is available.
     */
    uint32_t first = pdo[0];
    if ((first >> 30) != 0u || ((first >> 10) & 0x3ffu) != 100u)
        return PD_POWER_INVALID_CAPS;

    /* Target validation is new but cannot affect the pinned 7 V / 1 A input.
     * Never truncate non-representable requests into apparently valid RDOs.
     */
    if (!target_mv || !target_ma || (target_mv % 20u) != 0u ||
        (target_ma % 50u) != 0u ||
        (target_mv / 20u) > 0x7ffu || (target_ma / 50u) > 0x7fu)
        return PD_POWER_INVALID_TARGET;

    for (uint8_t i = 0u; i < count; ++i) {
        uint32_t v = pdo[i];
        if ((v >> 30) != 3u || ((v >> 28) & 3u) != 0u) continue;
        uint32_t min_mv = ((v >> 8) & 0xffu) * 100u;
        uint32_t max_mv = ((v >> 17) & 0xffu) * 100u;
        uint32_t max_ma = (v & 0x7fu) * 50u;
        if (target_mv < min_mv || target_mv > max_mv || target_ma > max_ma)
            continue;

        pd_power_plan_t selected;
        selected.object_pos = (uint8_t)(i + 1u);
        selected.rdo = ((uint32_t)selected.object_pos << 28) |
                       PD_POWER_RDO_FLAGS |
                       ((target_mv / 20u) << 9) | (target_ma / 50u);
        selected.pps = true;
        *out = selected;
        return PD_POWER_PPS;
    }

    /* The old sink fallback is always fixed 5 V at 1 A, regardless of
     * requested target. The application-level API will choose its own
     * explicit "unsupported target" behavior in a later change. */
    uint32_t advertised_ma = (first & 0x3ffu) * 10u;
    if (advertised_ma < 1000u) return PD_POWER_UNAVAILABLE;

    pd_power_plan_t selected;
    selected.object_pos = 1u;
    selected.rdo = PD_POWER_FIXED_5V_RDO;
    selected.pps = false;
    *out = selected;
    return PD_POWER_FIXED_5V;
}
