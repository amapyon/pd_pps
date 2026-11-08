/* Real monitor + observer with a stub ADC, no device register writes. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "pd_vbus_monitor.h"
static bool ready;
static uint16_t raw_value;
static uint32_t samples;
bool adc_voltage_start_async(void) { return true; }
bool adc_voltage_poll_async(uint16_t *raw) {
    if (!ready) return false;
    ready = false;
    *raw = raw_value;
    return true;
}
uint32_t adc_voltage_vbus_mv(uint16_t raw) { return (uint32_t)raw * 9u; }
static void feed(uint32_t t, uint16_t raw, bool reset) {
    pd_vbus_monitor_tick(t, true, false, reset);
    assert(pd_vbus_monitor_busy());
    ready = true;
    raw_value = raw;
    pd_vbus_monitor_tick(t+1u, true, false, reset);
    samples++;
}
int main(void) {
    pd_vbus_monitor_init();
    pd_vbus_monitor_attach(0);
    feed(0, 791, false);
    feed(10, 792, false);
    assert(pd_vbus_monitor_hr_observation()->state == PD_HR_IDLE);
    pd_vbus_monitor_hr_begin(20);
    assert(pd_vbus_monitor_hr_observation()->state == PD_HR_WAIT_LOW);
    /* Hard Reset still latched; no GoodCRC and no PE contract restoration.
       The observer sees 0V and 5V in order. */
    feed(20, 16, true);
    feed(30, 16, true);
    feed(40, 17, true);
    assert(pd_vbus_monitor_hr_observation()->state == PD_HR_WAIT_5V);
    feed(50, 792, true);
    feed(60, 572, true);
    feed(70, 573, true);
    feed(80, 573, true);
    assert(pd_vbus_monitor_hr_observation()->state == PD_HR_5V_OBSERVED);
    assert(!pd_vbus_monitor_loss_detected());
    pd_vbus_monitor_detach(100);
    assert(pd_vbus_monitor_hr_observation()->state == PD_HR_5V_OBSERVED);
    assert(samples == 9);
    /* Fresh physical attach starts a new, empty observer. */
    pd_vbus_monitor_attach(200);
    assert(pd_vbus_monitor_hr_observation()->state == PD_HR_IDLE);

    /* If reset arrives between conversion START and EOC, the old sample
       (pending_flags missing RESET) must not satisfy low. */
    pd_vbus_monitor_tick(200, true, true, false);
    pd_vbus_monitor_hr_begin(200);
    ready = true; raw_value = 16;
    pd_vbus_monitor_tick(201, true, false, true);
    assert(pd_vbus_monitor_hr_observation()->state == PD_HR_WAIT_LOW);
    assert(pd_vbus_monitor_hr_observation()->streak == 0);

    /* ADC 3ms timeout is a terminal observation error, never safe recovery. */
    pd_vbus_monitor_tick(210, true, false, true);
    assert(pd_vbus_monitor_busy());
    pd_vbus_monitor_tick(213, true, false, true);
    assert(pd_vbus_monitor_hr_observation()->state == PD_HR_FAILED);
    assert(pd_vbus_monitor_hr_observation()->error == 3);
    puts("M13a-4 integrated monitor simulation: PASS");
    return 0;
}
