#ifndef PD_HR_RECOVERY_H
#define PD_HR_RECOVERY_H
#include <stdbool.h>
#include "pd_hr_vbus.h"

/* M13b: ONE received Hard Reset recovery per physical CC attachment.
 * A second Hard Reset (or an ADC/voltage failure) stays fail-closed.
 * This conservative guard is NOT a complete USB PD HardResetCounter. */
typedef struct {
    bool used;
    bool active;
    bool early_rx_armed;
    bool completed;
    bool failed;
    bool gate_waiting;
    uint8_t blocked_gate;
    uint32_t blocked_since_ms;
} pd_hr_recovery_t;
typedef enum {
    PD_HR_REC_NONE = 0, PD_HR_REC_ARM_RX,
    PD_HR_REC_COMMIT, PD_HR_REC_STOP
} pd_hr_recovery_action_t;
void pd_hr_recovery_init(pd_hr_recovery_t *r);
/* Only on a confirmed NEW physical CC attach; do not call for Hard Reset. */
void pd_hr_recovery_new_session(pd_hr_recovery_t *r);
bool pd_hr_recovery_begin(pd_hr_recovery_t *r);
pd_hr_recovery_action_t pd_hr_recovery_step(const pd_hr_recovery_t *r,
    const pd_hr_vbus_t *observer, bool cc, bool phy_reset, bool tx_timeout);
void pd_hr_recovery_armed(pd_hr_recovery_t *r);
void pd_hr_recovery_completed(pd_hr_recovery_t *r);
void pd_hr_recovery_failed(pd_hr_recovery_t *r);
/* Keep the one-shot boot latch but stop monitoring on physical detach. */
void pd_hr_recovery_detached(pd_hr_recovery_t *r);
/* A transient busy GoodCRC can delay a PHY transition. Retry for at most
 * 20ms from the first refusal; an indefinitely busy gate is fail-closed.
 * Returns true iff a refusal has exceeded the budget. */
#define PD_HR_REC_GATE_TIMEOUT_MS 20u
bool pd_hr_recovery_gate_busy(pd_hr_recovery_t *r,
                              pd_hr_recovery_action_t action,
                              uint32_t now_ms);
void pd_hr_recovery_gate_ready(pd_hr_recovery_t *r);
#endif
