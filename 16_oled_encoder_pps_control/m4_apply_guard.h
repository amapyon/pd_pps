#ifndef M4_APPLY_GUARD_H
#define M4_APPLY_GUARD_H

#include <stdbool.h>
#include <stdint.h>
#include "pps_apdo_limits.h"

/* No device access here: deterministic policy, testable on a PC.
 * The caller must compare the advertised APDO PDO slot and fields against
 * the snapshot taken when UI editing began. Request current is a PD RDO
 * request, NOT a physical current regulator or measured load current. */
typedef struct {
    bool pd_ready;
    bool original_target_unchanged;
    bool source_apdo_unchanged;
    bool source_power_limited;
    PPSApdoLimits apdo;
    uint32_t mv;
    uint32_t ma;
} m4_apply_check_t;

typedef enum {
    M4_APPLY_OK = 0,
    M4_APPLY_PD_NOT_READY,
    M4_APPLY_TARGET_CHANGED,
    M4_APPLY_SOURCE_CHANGED,
    M4_APPLY_POWER_LIMITED,
    M4_APPLY_OUT_OF_RANGE,
    M4_APPLY_LOW_REQUEST
} m4_apply_verdict_t;

m4_apply_verdict_t m4_apply_validate(const m4_apply_check_t *check);
const char *m4_apply_message(m4_apply_verdict_t verdict);

#endif
