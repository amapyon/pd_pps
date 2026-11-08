#ifndef PD_HR_PPS_PROBE_H
#define PD_HR_PPS_PROBE_H
#include <stdbool.h>
#include <stdint.h>
#include "pd_policy.h"

/* M13a-7: one informational Source_Capabilities reprobe after the first
 * Hard Reset recovery falls back to an explicit fixed-5V contract.
 * This experiment does not repeatedly poll a Source or synthesize PDOs. */
#define PD_HR_PPS_PROBE_READY_DELAY_MS 1000u
#define PD_HR_PPS_PROBE_TX_START_TIMEOUT_MS 200u
#define PD_HR_PPS_PROBE_RESPONSE_TIMEOUT_MS 250u

typedef enum {
    PD_HR_PPS_PROBE_IDLE = 0,
    PD_HR_PPS_PROBE_DELAY,
    PD_HR_PPS_PROBE_TX_QUEUED,
    PD_HR_PPS_PROBE_WAIT_CAPS,
    PD_HR_PPS_PROBE_FIXED_ONLY,
    PD_HR_PPS_PROBE_PPS_OFFERED,
    PD_HR_PPS_PROBE_PPS_READY,
    PD_HR_PPS_PROBE_NO_RESPONSE,
    PD_HR_PPS_PROBE_ABORTED,
    PD_HR_PPS_PROBE_PPS_ALREADY_READY
} pd_hr_pps_probe_state_t;

typedef struct {
    pd_hr_pps_probe_state_t state;
    uint32_t ready_start_ms;
    uint32_t request_start_ms;
    bool sent;
    bool caps_seen;
    bool pps_offered;
    bool pps_ready;
    bool timeout;
    bool ready_timer_valid;
} pd_hr_pps_probe_t;

void pd_hr_pps_probe_init(pd_hr_pps_probe_t *p);
/* Returns true only while eligible to schedule the sole Get_Source_Cap.
 * Calling this function does NOT change PE state. */
bool pd_hr_pps_probe_step(pd_hr_pps_probe_t *p, uint32_t now,
                          bool recovery_complete, bool attached,
                          bool phy_blocked, const pe_t *pe);
/* Invoke only after pe_request_source_caps() accepts the command. */
void pd_hr_pps_probe_queued(pd_hr_pps_probe_t *p, uint32_t now);
/* Invoke only when phy_send(GET_SOURCE_CAP) accepts the command. */
void pd_hr_pps_probe_tx_started(pd_hr_pps_probe_t *p, uint32_t now);
/* Invoke AFTER pe_event(PE_EV_CAPS), using the actual PE evaluation. */
void pd_hr_pps_probe_caps(pd_hr_pps_probe_t *p, const pe_t *pe);

#endif
