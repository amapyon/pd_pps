#ifndef PD17_M13A2_ADC_MOCK_H
#define PD17_M13A2_ADC_MOCK_H
#include <stdint.h>
typedef struct {
    volatile uint32_t APB2PCENR, APB2PRSTR;
} mock_adc_rcc_t;
typedef struct { volatile uint32_t CFGLR; } mock_adc_gpio_t;
typedef struct {
    volatile uint32_t CTLR2, CTLR3, STATR, RDATAR;
    volatile uint32_t RSQR1, RSQR2, RSQR3, SAMPTR2;
} mock_adc_dev_t;
extern mock_adc_rcc_t mock_adc_rcc;
extern mock_adc_gpio_t mock_adc_gpio;
extern mock_adc_dev_t mock_adc;
#define RCC (&mock_adc_rcc)
#define GPIOA (&mock_adc_gpio)
#define ADC1 (&mock_adc)
#define RCC_APB2Periph_GPIOA 1u
#define RCC_ADC1EN 2u
#define RCC_ADC1RST 4u
#define ADC_CTLR3_CLK_DIV 0x30u
#define ADC_CTLR3_CLK_DIV_3 0x10u
#define ADC_ADON 1u
#define ADC_EXTSEL 2u
#define ADC_RSTCAL 4u
#define ADC_CAL 8u
#define ADC_EOC 16u
#define ADC_SWSTART 32u
void Delay_Ms(uint32_t ms);
#endif
