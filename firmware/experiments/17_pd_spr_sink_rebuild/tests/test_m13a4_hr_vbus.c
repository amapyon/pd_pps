#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "pd_hr_vbus.h"

static void sample(pd_hr_vbus_t *h, uint32_t at, uint16_t raw)
{
    pd_hr_vbus_sample(h, at, raw);
}
int main(void)
{
    pd_hr_vbus_t h;
    pd_hr_vbus_init(&h);
    assert(h.state == PD_HR_IDLE);
    sample(&h, 5, 16);
    assert(h.state == PD_HR_IDLE);

    /* Contract must never be resumed just because VBUS is already ~5V. */
    pd_hr_vbus_begin(&h, 100);
    sample(&h, 110, 570);
    sample(&h, 120, 570);
    sample(&h, 130, 570);
    assert(h.state == PD_HR_WAIT_LOW);

    /* 0V input must be consecutive; 3 samples before entering WAIT_5V. */
    sample(&h, 140, 17);
    sample(&h, 150, 17);
    sample(&h, 160, 792);
    assert(h.state == PD_HR_WAIT_LOW);
    sample(&h, 170, 17);
    sample(&h, 180, 16);
    assert(h.state == PD_HR_WAIT_LOW);
    sample(&h, 190, 15);
    assert(h.state == PD_HR_WAIT_5V && h.low_time_ms == 190);
    assert(h.low_raw == 15);
    sample(&h, 200, 792); /* 7V is not 5V */
    sample(&h, 210, 16);
    sample(&h, 220, 550);
    sample(&h, 230, 560);
    assert(h.state == PD_HR_WAIT_5V);
    sample(&h, 240, 580);
    assert(h.state == PD_HR_5V_OBSERVED);
    assert(h.low_seen && h.high_seen);
    assert(h.high_raw == 580 && h.high_time_ms == 240);
    sample(&h, 250, 792);
    pd_hr_vbus_tick(&h, 10000);
    assert(h.state == PD_HR_5V_OBSERVED); /* permanently diagnostic */
    pd_hr_vbus_detach(&h);
    assert(h.state == PD_HR_5V_OBSERVED); /* report still has completed result */

    /* Thresholds inclusive, but never accept 0.8V as proof with an
       uncalibrated +0.15V offset near zero. */
    pd_hr_vbus_begin(&h, 0);
    sample(&h, 10, PD_HR_VBUS_LOW_RAW_MAX);
    sample(&h, 20, PD_HR_VBUS_LOW_RAW_MAX);
    sample(&h, 30, PD_HR_VBUS_LOW_RAW_MAX);
    assert(h.state == PD_HR_WAIT_5V);
    sample(&h, 40, PD_HR_VBUS_5V_RAW_MIN);
    sample(&h, 50, PD_HR_VBUS_5V_RAW_MAX);
    sample(&h, 60, PD_HR_VBUS_5V_RAW_MIN);
    assert(h.state == PD_HR_5V_OBSERVED);

    /* A long gap breaks the consecutive sample streak even if all low. */
    pd_hr_vbus_begin(&h, 0);
    sample(&h, 10, 17);
    sample(&h, 20, 17);
    sample(&h, 100, 17);
    assert(h.state == PD_HR_WAIT_LOW && h.streak == 1u);
    sample(&h, 110, 17);
    sample(&h, 120, 17);
    assert(h.state == PD_HR_WAIT_5V);

    /* No low transition before timeout => fail closed, no late recovery. */
    pd_hr_vbus_begin(&h, 0);
    pd_hr_vbus_tick(&h, PD_HR_VBUS_LOW_TIMEOUT_MS - 1);
    assert(h.state == PD_HR_WAIT_LOW);
    pd_hr_vbus_tick(&h, PD_HR_VBUS_LOW_TIMEOUT_MS);
    assert(h.state == PD_HR_FAILED && h.error == 1);
    assert(!h.low_seen && !h.high_seen);
    sample(&h, 1010, 16);
    sample(&h, 1020, 16);
    sample(&h, 1030, 16);
    assert(h.state == PD_HR_FAILED);
    pd_hr_vbus_detach(&h);
    assert(h.state == PD_HR_FAILED);

    /* Low detected, but missing stable 5V => second timeout. */
    pd_hr_vbus_begin(&h, 500);
    sample(&h, 510, 17);
    sample(&h, 520, 17);
    sample(&h, 530, 17);
    assert(h.state == PD_HR_WAIT_5V);
    pd_hr_vbus_tick(&h, 530 + PD_HR_VBUS_HIGH_TIMEOUT_MS);
    assert(h.state == PD_HR_FAILED && h.error == 2);

    /* ADC failure must inhibit the observer and any subsequent samples. */
    pd_hr_vbus_begin(&h, 0);
    pd_hr_vbus_adc_failed(&h);
    assert(h.state == PD_HR_FAILED && h.error == 3);
    sample(&h, 10, 16);
    assert(h.state == PD_HR_FAILED);
    pd_hr_vbus_begin(&h, 0);
    pd_hr_vbus_detach(&h);
    assert(h.state == PD_HR_DETACHED);
    sample(&h, 20, 17);
    assert(h.state == PD_HR_DETACHED);

    /* Large millis wrap is safe with unsigned subtract. */
    pd_hr_vbus_begin(&h, 0xffffffecu);
    sample(&h, 0xfffffff6u, 15);
    sample(&h, 0u, 15);
    sample(&h, 10u, 15);
    assert(h.state == PD_HR_WAIT_5V && h.low_time_ms == 10u);
    sample(&h, 20u, 572);
    sample(&h, 30u, 572);
    sample(&h, 40u, 572);
    assert(h.state == PD_HR_5V_OBSERVED);

    puts("M13a-4 pure Hard Reset VBUS observer: PASS");
    return 0;
}
