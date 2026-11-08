#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "pd_sensors.h"
#include "adc_calibration.h"
#include "ina226_units.h"

int main(void)
{
    assert(!pd_sensors_ready_due(false, true, true, 299, 0));
    assert(pd_sensors_ready_due(false, true, true, 300, 0));
    assert(!pd_sensors_ready_due(true, true, true, 301, 0));
    assert(!pd_sensors_ready_due(false, false, true, 301, 0));
    assert(!pd_sensors_ready_due(false, true, false, 301, 0));
    assert(pd_sensors_ready_due(false, true, true, 0x00000011u, 0xfffffeddu));
    /* Only the calibrated raw range may be displayed as calibrated. */
    assert(!adc_calibration_in_range(0));
    assert(!adc_calibration_in_range(456));
    assert(adc_calibration_in_range(457));
    assert(adc_calibration_in_range(1012));
    assert(!adc_calibration_in_range(1013));
    assert(ina226_bus_mv(5600) == 7000);
    assert(ina226_r010_current_ua(4) == 1000);
    puts("M13A sensor scheduling and unit conversion: PASS");
    return 0;
}
