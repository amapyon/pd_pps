#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "adc_calibration.h"
int main(void)
{
    const uint16_t raw[] = {457, 569, 680, 791, 901, 1012};
    const uint32_t dmm_mv[] = {4000, 5010, 6020, 7030, 8030, 9040};
    for (unsigned i = 0; i < sizeof(raw)/sizeof(raw[0]); i++) {
        const uint32_t v = adc_calibration_vbus_mv(raw[i]);
        const int32_t delta = (int32_t)v - (int32_t)dmm_mv[i];
        assert(delta >= -15 && delta <= 15);
        assert(adc_calibration_in_range(raw[i]));
        printf("raw=%u calibrated=%lu mV DMM=%lu mV delta=%ld mV\n",
               raw[i], (unsigned long)v, (unsigned long)dmm_mv[i], (long)delta);
    }
    assert(!adc_calibration_in_range(456));
    assert(!adc_calibration_in_range(1013));
    assert(adc_calibration_vbus_mv(0)==0);
    puts("ADC calibration test: PASS");
    return 0;
}
