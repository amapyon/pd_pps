#ifndef PD_SENSORS_H
#define PD_SENSORS_H
#include <stdbool.h>
#include <stdint.h>

/* M13a sensing bring-up: observability only. This module MUST NOT be used
 * to gate Hard Reset recovery until near-zero and 5 V thresholds have been
 * validated for this exact divider, power domain and board.
 * Never call measurement functions from the USBPD IRQ handler.
 */
typedef struct {
    uint16_t adc_raw;
    uint32_t adc_uncal_mv;
    uint32_t adc_cal_mv;
    bool adc_ok;
    bool adc_calibrated_range;
    bool ina_ok;
    uint32_t ina_bus_mv;
    int32_t ina_current_ua;
    int32_t ina_power_mw;
} pd_sensor_reading_t;

void pd_sensors_init(void);
void pd_sensors_capture_ready(void);
void pd_sensors_capture_detached(void);
void pd_sensors_clear(void);
void pd_sensors_report_detached(void); /* printf: after confirmed disconnect ONLY */

/* Small, pure predicate for predictable scheduling / host test. */
bool pd_sensors_ready_due(bool already_sampled, bool attached,
                          bool pe_ready, uint32_t now_ms,
                          uint32_t first_ready_ms);
#endif
