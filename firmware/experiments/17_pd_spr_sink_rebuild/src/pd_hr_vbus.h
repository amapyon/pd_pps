#ifndef PD_HR_VBUS_H
#define PD_HR_VBUS_H
#include <stdbool.h>
#include <stdint.h>

/* M13a-4: OBSERVATION ONLY. No authorization for USB PD TX, GoodCRC,
 * Sink policy restart, or any external load. A power transition is not
 * considered verified merely because these ADC thresholds were crossed. */
#define PD_HR_VBUS_LOW_RAW_MAX 70u      /* ~0.62V (uncalibrated); below 0.8V vSafe0V(max) */
#define PD_HR_VBUS_5V_RAW_MIN 540u      /* ~4.78V (uncalibrated), conservative */
#define PD_HR_VBUS_5V_RAW_MAX 610u      /* ~5.40V (uncalibrated), conservative */
#define PD_HR_VBUS_STABLE_SAMPLES 3u
#define PD_HR_VBUS_MAX_SAMPLE_GAP_MS 25u
#define PD_HR_VBUS_LOW_TIMEOUT_MS 1000u
#define PD_HR_VBUS_HIGH_TIMEOUT_MS 2000u

typedef enum {
    PD_HR_IDLE = 0, PD_HR_WAIT_LOW, PD_HR_WAIT_5V,
    PD_HR_5V_OBSERVED, PD_HR_FAILED, PD_HR_DETACHED
} pd_hr_state_t;

typedef struct {
    pd_hr_state_t state;
    uint32_t started_ms;
    uint32_t low_time_ms;
    uint32_t high_time_ms;
    uint32_t state_start_ms;
    uint32_t last_sample_ms;
    uint16_t low_raw;
    uint16_t high_raw;
    uint8_t streak;
    bool seen_sample;
    bool low_seen;
    bool high_seen;
    uint8_t error;       /* 1=low timeout, 2=5V timeout, 3=ADC failure */
} pd_hr_vbus_t;

void pd_hr_vbus_init(pd_hr_vbus_t *h);
void pd_hr_vbus_begin(pd_hr_vbus_t *h, uint32_t now_ms);
void pd_hr_vbus_sample(pd_hr_vbus_t *h, uint32_t now_ms, uint16_t raw);
void pd_hr_vbus_tick(pd_hr_vbus_t *h, uint32_t now_ms);
void pd_hr_vbus_adc_failed(pd_hr_vbus_t *h);
void pd_hr_vbus_detach(pd_hr_vbus_t *h);
#endif
