#ifndef ADC_VOLTAGE_H
#define ADC_VOLTAGE_H
#include <stdint.h>
/* ADC_IN0 on PA0 (CH32X035F8U6 IC physical pin 2). */
void adc_voltage_init_pa0(void);
uint16_t adc_voltage_read_average(unsigned samples);
uint32_t adc_voltage_input_mv(uint16_t raw);
uint32_t adc_voltage_vbus_mv(uint16_t raw);
#endif
