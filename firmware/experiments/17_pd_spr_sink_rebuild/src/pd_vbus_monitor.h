#ifndef PD_VBUS_MONITOR_H
#define PD_VBUS_MONITOR_H
#include <stdbool.h>
#include <stdint.h>
#include "pd_vbus_history.h"
#include "pd_hr_vbus.h"

#define PD_VBUS_SAMPLE_PERIOD_MS 10u
#define PD_VBUS_ADC_TIMEOUT_MS 3u
/* Raw PA0 ADC values, not extrapolated calibration at near-zero VBUS.
 * 90 counts corresponds to ~0.80 V for 3.3V/12-bit/11:1 divider.
 * M13a-3 uses this for a conservative loss STOP, not for vSafe0V exit. */
#define PD_VBUS_LOSS_RAW_MAX 90u
#define PD_VBUS_LOSS_CONSECUTIVE 3u

/* M13a-3: detect sustained unexpected loss and latch a fail-closed stop.
 * This does NOT declare USB PD vSafe0V/vSafe5V or authorize recovery.
 * Previously diagnostic-only sampling remains active for the trace. */
/* Not used to declare vSafe0V/vSafe5V or to
 * authorize Hard Reset recovery. No I2C and no printf while connected. */
void pd_vbus_monitor_init(void);
void pd_vbus_monitor_attach(uint32_t now_ms);
void pd_vbus_monitor_detach(uint32_t now_ms);
void pd_vbus_monitor_tick(uint32_t now_ms, bool cc_attached,
                          bool ready, bool hard_reset);
bool pd_vbus_monitor_busy(void);
void pd_vbus_monitor_stop(void);
void pd_vbus_monitor_report_begin(void);
bool pd_vbus_monitor_report_step(void); /* one line/call; true after END */
void pd_vbus_monitor_clear(void);

/* Read-only data for simulation/host tests. */
const pd_vbus_history_t *pd_vbus_monitor_history(void);
uint32_t pd_vbus_monitor_timeouts(void);
bool pd_vbus_monitor_stopped_on_error(void);
/* One-shot latch: reading does not clear it. Cleared only on session reset.
 * Never use this alone for policy transitions without also checking the
 * physical CC and Hard Reset latch. */
/* Passive Hard Reset VBUS observer: never enables PHY or PE recovery. */
void pd_vbus_monitor_hr_begin(uint32_t now_ms);
const pd_hr_vbus_t *pd_vbus_monitor_hr_observation(void);
/* Track separate milestones: voltage-confirmed protocol handoff,
   explicit PD Ready recovery (including fixed 5V), and PPS recovery. */
void pd_vbus_monitor_hr_handoff(void);
void pd_vbus_monitor_hr_recovered(void); /* legacy PPS-only marker */
void pd_vbus_monitor_hr_contract_ready(bool pps);
bool pd_vbus_monitor_loss_detected(void);
uint16_t pd_vbus_monitor_loss_raw(void);
uint32_t pd_vbus_monitor_loss_time_ms(void);
#endif
