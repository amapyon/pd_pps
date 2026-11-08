/* Derived from the user-tested CH32X035 standalone ADC VBUS monitor.
 * ADC_IN0 PA0: physical IC pin 2. Divider VBUS -> 100k -> PA0 -> 10k -> GND.
 * Supports VBUS range only within hardware/ADC input limitations.
 * 3300mV ADC reference is ASSUMED; no calibration is applied.
 */
#include "ch32fun.h"
#include "adc_voltage.h"
#define ADC_VREF_MV 3300u
#define ADC_FULL_SCALE 4095u
#define R_TOP_OHM 100000u
#define R_BOTTOM_OHM 10000u
#define ADC_READ_TIMEOUT 1000000u

void adc_voltage_init_pa0(void)
{
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_ADC1EN;
    GPIOA->CFGLR &= ~(0xFu << (4u * 0u));
    RCC->APB2PRSTR |= RCC_ADC1RST;
    RCC->APB2PRSTR &= ~RCC_ADC1RST;
    ADC1->CTLR3 &= ~ADC_CTLR3_CLK_DIV;
    ADC1->CTLR3 |= ADC_CTLR3_CLK_DIV_3;
    ADC1->RSQR1 = 0;
    ADC1->RSQR2 = 0;
    ADC1->RSQR3 = 0;
    ADC1->SAMPTR2 = (ADC1->SAMPTR2 & ~7u) | 7u;
    ADC1->CTLR2 |= ADC_ADON | ADC_EXTSEL;
    Delay_Ms(2);
    ADC1->CTLR2 |= ADC_RSTCAL;
    while (ADC1->CTLR2 & ADC_RSTCAL) { }
    ADC1->CTLR2 |= ADC_CAL;
    while (ADC1->CTLR2 & ADC_CAL) { }
}

static uint16_t adc_voltage_read_one(void)
{
    ADC1->STATR &= ~ADC_EOC;
    ADC1->CTLR2 |= ADC_SWSTART;
    uint32_t timeout = ADC_READ_TIMEOUT;
    while (!(ADC1->STATR & ADC_EOC)) {
        if (--timeout == 0u) return 0xffffu;
    }
    return (uint16_t)(ADC1->RDATAR & 0x0fffu);
}

uint16_t adc_voltage_read_average(unsigned samples)
{
    if (samples == 0u) return 0xffffu;
    uint32_t sum = 0;
    for (unsigned i = 0; i < samples; ++i) {
        uint16_t value = adc_voltage_read_one();
        if (value == 0xffffu) return 0xffffu;
        sum += value;
    }
    return (uint16_t)((sum + samples / 2u) / samples);
}
uint32_t adc_voltage_input_mv(uint16_t raw)
{
    return ((uint32_t)raw * ADC_VREF_MV + ADC_FULL_SCALE / 2u) / ADC_FULL_SCALE;
}
uint32_t adc_voltage_vbus_mv(uint16_t raw)
{
    uint64_t n = (uint64_t)raw * ADC_VREF_MV * (R_TOP_OHM + R_BOTTOM_OHM);
    uint64_t d = (uint64_t)ADC_FULL_SCALE * R_BOTTOM_OHM;
    return (uint32_t)((n + d / 2u) / d);
}
