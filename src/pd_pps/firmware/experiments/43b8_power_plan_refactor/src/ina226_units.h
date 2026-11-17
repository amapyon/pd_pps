#ifndef INA226_UNITS_H
#define INA226_UNITS_H

#include <stdint.h>

/* R010 = 0.010 ohm, INA226 shunt LSB = 2.5 uV.
 * current = raw * 2.5uV / 0.010 ohm = raw * 250 uA.
 * Do not substitute the uncalibrated INA226 Current Register here.
 */
static inline int32_t ina226_r010_current_ua(int16_t shunt_raw)
{
    return (int32_t)shunt_raw * 250;
}

/* INA226 VBUS voltage register LSB = 1.25 mV. Rounded to mV. */
static inline uint32_t ina226_bus_mv(uint16_t bus_raw)
{
    return ((uint32_t)bus_raw * 5u + 2u) / 4u;
}

/* Rounded signed power in mW (positive = power delivered to load).
 * No floating point required, suitable for CH32X035.
 */
static inline int32_t ina226_power_mw(uint32_t bus_mv, int32_t current_ua)
{
    int64_t numerator = (int64_t)bus_mv * (int64_t)current_ua;
    if (numerator >= 0) numerator += 500000LL;
    else numerator -= 500000LL;
    return (int32_t)(numerator / 1000000LL);
}

#endif
