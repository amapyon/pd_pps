#ifndef ADC_CALIBRATION_H
#define ADC_CALIBRATION_H
#include <stdint.h>
/* Empirical fit against DMM readings for THIS board/divider, 4.00..9.04 V.
 * 6 point least squares: VBUS[mV] = 9.08901162 * raw - 156.48462433.
 * Results outside the calibration range are EXTRAPOLATIONS, not verified.
 */
#define ADC_CAL_RAW_MIN 457u
#define ADC_CAL_RAW_MAX 1012u
uint32_t adc_calibration_vbus_mv(uint16_t raw);
int adc_calibration_in_range(uint16_t raw);
#endif
