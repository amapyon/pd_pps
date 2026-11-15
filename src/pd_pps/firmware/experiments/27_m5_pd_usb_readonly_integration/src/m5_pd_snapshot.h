#ifndef M5_PD_SNAPSHOT_H
#define M5_PD_SNAPSHOT_H

#include <stdbool.h>
#include <stdint.h>
/* Use the exact types from the existing, untouched PD17 experiment. */
#include "../../17_pd_spr_sink_rebuild/src/pd_policy.h"
#include "../../17_pd_spr_sink_rebuild/src/pd_sensors.h"

/*
 * Host-only design bridge. Main loop ONLY; do not call in USBPD/USBFS IRQ.
 * Do not send Policy Engine commands, sample INA226, or touch peripherals.
 */
typedef struct {
    pe_state_t state;
    uint8_t caps_epoch;
    bool caps_valid;
    uint8_t caps_count;
    uint32_t raw_pdos[7];

    bool contract_valid; /* true only when PE_READY and fresh consistent caps */
    bool pps_active;
    uint8_t pdo_index;   /* 1-based only when contract_valid */
    uint32_t requested_mv;
    uint32_t requested_ma; /* Source *request*, not a measured current */
    uint16_t faults;

    bool ina_valid;
    uint32_t ina_bus_mv;
    int32_t ina_current_ua; /* measured, SIGNED microamps */
    int32_t ina_power_mw;
    bool adc_valid;
    uint32_t adc_bus_mv;
} m5_pd_snapshot_t;

/*
 * Returns false on null arguments, invalid PE state or corrupt PDO count.
 * On false: out contains detached/invalid data, never old readings.
 * sensor_fresh is supplied by the main loop, not inferred from INA226's
 * latched 'ina_ok' flag: it must become false on detach/recovery or aging.
 */
bool m5_pd_snapshot_capture(const pe_t *pe, const pd_sensor_reading_t *sensor,
                            bool sensor_fresh, m5_pd_snapshot_t *out);
#endif
