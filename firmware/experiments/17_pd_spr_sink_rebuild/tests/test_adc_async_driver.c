#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include "ch32fun.h"
#include "adc_voltage.h"
mock_adc_rcc_t mock_adc_rcc;
mock_adc_gpio_t mock_adc_gpio;
mock_adc_dev_t mock_adc;
void Delay_Ms(uint32_t ms) { (void)ms; }
int main(void)
{
    uint16_t raw = 0;
    mock_adc.STATR = ADC_EOC; /* stale completion, must clear */
    assert(adc_voltage_start_async());
    assert(!(mock_adc.STATR & ADC_EOC));
    assert(mock_adc.CTLR2 & ADC_SWSTART);
    assert(!adc_voltage_poll_async(&raw)); /* must not block */
    mock_adc.RDATAR = 791u;
    mock_adc.STATR = ADC_EOC;
    assert(adc_voltage_poll_async(&raw) && raw == 791u);
    assert(adc_voltage_vbus_mv(raw) == 7012u);
    assert(adc_voltage_input_mv(4095u) == 3300u);
    assert(adc_voltage_vbus_mv(0u) == 0u);
    puts("M13a-2 real ADC driver async API (register mock): PASS");
    return 0;
}
