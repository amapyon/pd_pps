#ifndef PD_POWER_H
#define PD_POWER_H
#include <stdbool.h>
#include <stdint.h>

/* Host-testable SPR PPS selection and RDO construction.
 * No registers, interrupts, allocations, logging, or policy state changes.
 * A select result is a candidate, NOT an established power contract.
 * Source capabilities are limited to the seven PDOs in the existing PE.
 */
#define PD_POWER_MAX_PDOS 7u

typedef enum {
    PD_POWER_PPS = 0,
    PD_POWER_FIXED_5V,
    PD_POWER_INVALID_CAPS,
    PD_POWER_INVALID_TARGET,
    PD_POWER_UNAVAILABLE
} pd_power_result_t;

typedef struct {
    uint32_t rdo;
    uint8_t object_pos; /* PD object position is 1-based. */
    bool pps;
} pd_power_plan_t;

/* Keep M43B-7 fallback semantics: prefer first matching SPR PPS APDO;
 * otherwise request fixed 5 V / 1 A only if PDO 1 advertises >=1 A.
 * The fixed fallback is intentionally NOT scaled by target_ma.
 * A future user-selected PPS request must separately define what happens
 * when the requested target is unavailable; this API is not an APPLY API.
 * On non-success, *out is not modified.
 */
pd_power_result_t pd_power_select(const uint32_t *pdo, uint8_t count,
                                  uint32_t target_mv, uint32_t target_ma,
                                  pd_power_plan_t *out);
#endif
