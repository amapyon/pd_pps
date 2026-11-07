#include "pd_hr_vbus.h"

static bool waiting(const pd_hr_vbus_t *h)
{
    return h->state == PD_HR_WAIT_LOW || h->state == PD_HR_WAIT_5V;
}
void pd_hr_vbus_init(pd_hr_vbus_t *h)
{
    *h = (pd_hr_vbus_t){0};
}
void pd_hr_vbus_begin(pd_hr_vbus_t *h, uint32_t now_ms)
{
    pd_hr_vbus_init(h);
    h->state = PD_HR_WAIT_LOW;
    h->started_ms = now_ms;
    h->state_start_ms = now_ms;
}
void pd_hr_vbus_tick(pd_hr_vbus_t *h, uint32_t now_ms)
{
    if (!waiting(h)) return;
    /* All timeouts are well within INT32_MAX; subtraction tolerates wrap. */
    uint32_t timeout = h->state == PD_HR_WAIT_LOW ?
                       PD_HR_VBUS_LOW_TIMEOUT_MS : PD_HR_VBUS_HIGH_TIMEOUT_MS;
    if ((uint32_t)(now_ms - h->state_start_ms) >= timeout) {
        h->error = h->state == PD_HR_WAIT_LOW ? 1u : 2u;
        h->state = PD_HR_FAILED;
        h->streak = 0u;
    }
}
void pd_hr_vbus_sample(pd_hr_vbus_t *h, uint32_t now_ms, uint16_t raw)
{
    pd_hr_vbus_tick(h, now_ms);
    if (!waiting(h)) return;
    /* Stable means consecutive AND timely. A stalled ADC or foreground
       must not turn widely separated readings into a valid transition. */
    if (h->seen_sample &&
        (uint32_t)(now_ms - h->last_sample_ms) > PD_HR_VBUS_MAX_SAMPLE_GAP_MS)
        h->streak = 0u;
    h->last_sample_ms = now_ms;
    h->seen_sample = true;
    if (h->state == PD_HR_WAIT_LOW) {
        if (raw <= PD_HR_VBUS_LOW_RAW_MAX) {
            if (++h->streak >= PD_HR_VBUS_STABLE_SAMPLES) {
                h->state = PD_HR_WAIT_5V;
                h->low_seen = true;
                h->low_time_ms = now_ms;
                h->low_raw = raw;
                h->state_start_ms = now_ms;
                h->streak = 0u;
            }
        } else h->streak = 0u;
    } else {
        if (raw >= PD_HR_VBUS_5V_RAW_MIN && raw <= PD_HR_VBUS_5V_RAW_MAX) {
            if (++h->streak >= PD_HR_VBUS_STABLE_SAMPLES) {
                h->state = PD_HR_5V_OBSERVED;
                h->high_seen = true;
                h->high_time_ms = now_ms;
                h->high_raw = raw;
                h->streak = 0u;
            }
        } else h->streak = 0u;
    }
}
void pd_hr_vbus_adc_failed(pd_hr_vbus_t *h)
{
    if (waiting(h)) {
        h->state = PD_HR_FAILED;
        h->error = 3u;
        h->streak = 0u;
    }
}
void pd_hr_vbus_detach(pd_hr_vbus_t *h)
{
    /* Preserve any completed observation or error for detached-only logs. */
    if (waiting(h)) {
        h->state = PD_HR_DETACHED;
        h->streak = 0u;
    }
}
