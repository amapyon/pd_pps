/* White-box monitor test: distinguish explicit fixed-5V vs PPS recovery. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "adc_voltage.h"

bool adc_voltage_start_async(void) { return true; }
bool adc_voltage_poll_async(uint16_t *raw) { (void)raw; return false; }
uint32_t adc_voltage_vbus_mv(uint16_t raw) { return (uint32_t)raw * 9u; }
#include "../src/pd_vbus_monitor.c"

int main(void) {
    pd_vbus_monitor_init();
    pd_vbus_monitor_hr_contract_ready(true);
    assert(!hr_autorecovered && !hr_pps_recovered); /* No handoff */
    pd_vbus_monitor_hr_handoff();
    pd_vbus_monitor_hr_contract_ready(false);
    assert(hr_autorecovered && !hr_pps_recovered); /* Source fixed-5V */
    pd_vbus_monitor_hr_contract_ready(true);
    assert(hr_autorecovered && hr_pps_recovered); /* Later PPS */
    pd_vbus_monitor_clear();
    assert(!hr_autorecovered && !hr_pps_recovered && !hr_handoff);
    pd_vbus_monitor_hr_handoff();
    pd_vbus_monitor_hr_contract_ready(true);
    assert(hr_autorecovered && hr_pps_recovered);
    puts("M13b HR result flags: PASS (fixed5 ready != PPS ready)");
    return 0;
}
