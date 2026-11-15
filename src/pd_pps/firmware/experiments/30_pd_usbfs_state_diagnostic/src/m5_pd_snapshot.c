#include "m5_pd_snapshot.h"
#include <string.h>

bool m5_pd_snapshot_capture(const pe_t *pe, const pd_sensor_reading_t *sensor,
                            bool sensor_fresh, m5_pd_snapshot_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    out->state = PE_DETACHED;
    if (!pe) return false;

    if (pe->state < PE_DETACHED || pe->state > PE_RECOVERY ||
        pe->caps.count > 7) return false;

    out->state = pe->state;
    out->caps_epoch = pe->caps_epoch;
    out->faults = pe->faults;

    /* PD17's source capabilities are only visible while marked fresh. */
    if (pe->state != PE_DETACHED && pe->state != PE_RECOVERY &&
        pe->caps_fresh && pe->caps.count > 0) {
        out->caps_valid = true;
        out->caps_count = pe->caps.count;
        memcpy(out->raw_pdos, pe->caps.pdo, (size_t)out->caps_count * sizeof(uint32_t));
    }

    /* Transitional states can contain an old/new RDO; never show an
     * unconfirmed request as an active contract. READY is intentionally
     * stricter than generic pe->contract_valid. */
    if (pe->state == PE_READY && pe->contract_valid && out->caps_valid &&
        pe->pdo_index > 0 && pe->pdo_index <= out->caps_count &&
        pe->requested_mv != 0 && pe->requested_ma != 0) {
        out->contract_valid = true;
        out->pps_active = pe->pps_active;
        out->pdo_index = pe->pdo_index;
        out->requested_mv = pe->requested_mv;
        out->requested_ma = pe->requested_ma;
    }

    /* Sensor data is separately fresh/valid, never borrowed from PD request.
     * A failed/detached policy invalidates the snapshot's measurements. */
    if (sensor && sensor_fresh && pe->state == PE_READY) {
        if (sensor->ina_ok) {
            out->ina_valid = true;
            out->ina_bus_mv = sensor->ina_bus_mv;
            out->ina_current_ua = sensor->ina_current_ua;
            out->ina_power_mw = sensor->ina_power_mw;
        }
        if (sensor->adc_ok && sensor->adc_calibrated_range) {
            out->adc_valid = true;
            out->adc_bus_mv = sensor->adc_cal_mv;
        }
    }
    return true;
}
