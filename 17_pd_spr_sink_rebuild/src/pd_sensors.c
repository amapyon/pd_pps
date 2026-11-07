#include "pd_sensors.h"
#include "adc_voltage.h"
#include "adc_calibration.h"
#include "ina226.h"
#include <stdio.h>

#define READY_SAMPLE_DELAY_MS 300u
#define ADC_SAMPLE_COUNT 8u

static pd_sensor_reading_t ready_reading;
static pd_sensor_reading_t detached_reading;
static bool ready_attempted;
static bool detached_attempted;

bool pd_sensors_ready_due(bool already_sampled, bool attached,
                          bool pe_ready, uint32_t now_ms,
                          uint32_t first_ready_ms)
{
    return !already_sampled && attached && pe_ready &&
           (uint32_t)(now_ms - first_ready_ms) >= READY_SAMPLE_DELAY_MS;
}

static void adc_measure(pd_sensor_reading_t *r)
{
    uint16_t raw = adc_voltage_read_average(ADC_SAMPLE_COUNT);
    r->adc_ok = (raw != 0xffffu);
    if (!r->adc_ok) return;
    r->adc_raw = raw;
    r->adc_uncal_mv = adc_voltage_vbus_mv(raw);
    r->adc_calibrated_range = adc_calibration_in_range(raw) != 0;
    /* Calibrated values outside the measured range are NOT safe for use
       as vSafe0V or vSafe5V detectors. Display only when in range. */
    if (r->adc_calibrated_range)
        r->adc_cal_mv = adc_calibration_vbus_mv(raw);
}

void pd_sensors_init(void)
{
    adc_voltage_init_pa0();
    ina226_bus_init();
    pd_sensors_clear();
}

void pd_sensors_clear(void)
{
    ready_reading = (pd_sensor_reading_t){0};
    detached_reading = (pd_sensor_reading_t){0};
    ready_attempted = false;
    detached_attempted = false;
}

void pd_sensors_capture_ready(void)
{
    if (ready_attempted) return;
    ready_attempted = true;
    adc_measure(&ready_reading);
    INA226Reading ina;
    if (ina226_read(&ina)) {
        ready_reading.ina_ok = true;
        ready_reading.ina_bus_mv = ina.bus_mv;
        ready_reading.ina_current_ua = ina.current_ua;
        ready_reading.ina_power_mw = ina.power_mw;
    }
}

void pd_sensors_capture_detached(void)
{
    if (detached_attempted) return;
    detached_attempted = true;
    adc_measure(&detached_reading);
}

void pd_sensors_report_detached(void)
{
    if (ready_attempted) {
        printf("PD17 SENSOR READY adc_ok=%u raw=%u adc_uncal_mv=%lu cal_in_range=%u adc_cal_mv=%lu ina_ok=%u ina_bus_mv=%lu ina_current_ua=%ld ina_power_mw=%ld\n",
            (unsigned)ready_reading.adc_ok,
            (unsigned)ready_reading.adc_raw,
            (unsigned long)ready_reading.adc_uncal_mv,
            (unsigned)ready_reading.adc_calibrated_range,
            (unsigned long)ready_reading.adc_cal_mv,
            (unsigned)ready_reading.ina_ok,
            (unsigned long)ready_reading.ina_bus_mv,
            (long)ready_reading.ina_current_ua,
            (long)ready_reading.ina_power_mw);
    } else {
        printf("PD17 SENSOR READY not_sampled\n");
    }
    if (detached_attempted) {
        printf("PD17 SENSOR DETACHED adc_ok=%u raw=%u adc_uncal_mv=%lu\n",
            (unsigned)detached_reading.adc_ok,
            (unsigned)detached_reading.adc_raw,
            (unsigned long)detached_reading.adc_uncal_mv);
    } else {
        printf("PD17 SENSOR DETACHED not_sampled\n");
    }
}
