/* CH32X035F8U6 PA0 (IC physical pin 2), ADC_IN0.
 * Standalone test only. Never connect PPS VBUS directly to PA0.
 */
#include "ch32fun.h"
#include <stdint.h>
#include <stdio.h>

#define ADC_CHANNEL 0u
#define ASSUMED_VDD_MV 3300u  /* Placeholder, measure actual VDD for accuracy. */

static void adc_init_pa0(void)
{
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_ADC1EN;
    GPIOA->CFGLR &= ~(0xFu << (4u * 0u)); /* PA0 input analog */
    RCC->APB2PRSTR |= RCC_ADC1RST;
    RCC->APB2PRSTR &= ~RCC_ADC1RST;
    ADC1->CTLR3 &= ~ADC_CTLR3_CLK_DIV;
    ADC1->CTLR3 |= ADC_CTLR3_CLK_DIV_3;  /* /8 */
    ADC1->RSQR1 = 0;
    ADC1->RSQR2 = 0;
    ADC1->RSQR3 = ADC_CHANNEL;
    ADC1->SAMPTR2 = (ADC1->SAMPTR2 & ~7u) | 7u; /* longest sampling time */
    ADC1->CTLR2 |= ADC_ADON | ADC_EXTSEL;
    Delay_Ms(2);
    ADC1->CTLR2 |= ADC_RSTCAL;
    while (ADC1->CTLR2 & ADC_RSTCAL) { }
    ADC1->CTLR2 |= ADC_CAL;
    while (ADC1->CTLR2 & ADC_CAL) { }
}

static uint16_t adc_read_pa0(void)
{
    ADC1->STATR &= ~ADC_EOC;
    ADC1->CTLR2 |= ADC_SWSTART;
    while (!(ADC1->STATR & ADC_EOC)) { }
    return (uint16_t)(ADC1->RDATAR & 0x0FFFu);
}

int main(void)
{
    SystemInit();
    Delay_Ms(20);
    printf("CH32X035 ADC_IN0 PA0(IC pin 2)\n");
    adc_init_pa0();
    while (1) {
        uint32_t total = 0;
        for (int i = 0; i < 32; ++i) total += adc_read_pa0();
        uint32_t raw = (total + 16u) / 32u;
        uint32_t mv = (raw * ASSUMED_VDD_MV + 2047u) / 4095u;
        printf("ADC raw=%lu / 4095; PA0~%lu mV (assuming VDD=%u mV)\n",
               (unsigned long)raw, (unsigned long)mv, ASSUMED_VDD_MV);
        Delay_Ms(500);
    }
}
