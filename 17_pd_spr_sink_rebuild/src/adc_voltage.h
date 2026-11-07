#ifndef ADC_VOLTAGE_H
#define ADC_VOLTAGE_H
#include <stdint.h>
#include <stdbool.h>
/* ADC_IN0 on PA0 (CH32X035F8U6 IC physical pin 2). */
void adc_voltage_init_pa0(void);
/* Non-blocking one-conversion interface for diagnostic sampling. */
bool adc_voltage_start_async(void);
bool adc_voltage_poll_async(uint16_t *raw);
uint16_t adc_voltage_read_average(unsigned samples);
uint32_t adc_voltage_input_mv(uint16_t raw);
uint32_t adc_voltage_vbus_mv(uint16_t raw);
#endif
