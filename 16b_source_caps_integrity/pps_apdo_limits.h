#ifndef PPS_APDO_LIMITS_H
#define PPS_APDO_LIMITS_H
#include <stdbool.h>
#include <stdint.h>

/* SPR PPS APDO only. Values in mV/mA. The APDO reports a maximum
 * current, NOT a minimum current or a precision current-limit guarantee.
 * Reject out-of-range combinations rather than silently clamping them.
 */
typedef struct {
    uint32_t min_mv;
    uint32_t max_mv;
    uint32_t max_ma;
} PPSApdoLimits;

static inline bool pps_apdo_accepts(PPSApdoLimits apdo,
                                    uint32_t mv, uint32_t ma)
{
    return apdo.min_mv <= apdo.max_mv &&
           mv >= apdo.min_mv && mv <= apdo.max_mv &&
           (mv % 20u) == 0u &&
           ma >= 50u && ma <= apdo.max_ma && ma <= 6350u &&
           (ma % 50u) == 0u;
}

/* Decode raw SPR PPS APDO for regression testing against reported data;
 * main firmware uses the PD library's typed Source PDO fields.
 */
static inline bool pps_apdo_decode_raw(uint32_t raw, PPSApdoLimits *out)
{
    if (!out || ((raw >> 30u) & 3u) != 3u ||
        ((raw >> 28u) & 3u) != 0u)
        return false;
    out->min_mv = ((raw >> 8u) & 255u) * 100u;
    out->max_mv = ((raw >> 17u) & 255u) * 100u;
    out->max_ma = (raw & 127u) * 50u;
    return out->min_mv <= out->max_mv && out->max_ma >= 50u;
}
#endif
