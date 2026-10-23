#include "adc_calibration.h"
/* Integer fixed-point approximation, scale 1e6, signed intermediate.
 * Rounded to nearest mV for nonnegative results. Negative estimates clamp 0.
 */
uint32_t adc_calibration_vbus_mv(uint16_t raw)
{
    const int64_t scaled_mv = (int64_t)raw * 9089012LL - 156484624LL;
    if (scaled_mv <= 0) return 0u;
    return (uint32_t)((scaled_mv + 500000LL) / 1000000LL);
}
int adc_calibration_in_range(uint16_t raw)
{
    return raw >= ADC_CAL_RAW_MIN && raw <= ADC_CAL_RAW_MAX;
}
