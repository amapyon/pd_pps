#ifndef CONSOLE_PARSE_H
#define CONSOLE_PARSE_H
#include <stdbool.h>
#include <stdint.h>

/* Accept decimal volts ("7", "7.02", "11.0") or integer millivolts
 * ("7020", "11000"). Input must contain ASCII digits and optionally one
 * decimal point, with up to 2 fractional decimal digits.
 * Electrical/APDO validation is intentionally separate.
 */
static inline bool parse_voltage_mv(const char *s, uint32_t *mv)
{
    uint32_t integer = 0, fraction = 0, multiplier = 1;
    unsigned fractional_digits = 0;
    bool decimal = false, any = false;
    if (!s || !mv) return false;
    for (; *s; ++s) {
        char c = *s;
        if (c == '.') {
            if (decimal || !any) return false;
            decimal = true;
            continue;
        }
        if (c < '0' || c > '9') return false;
        unsigned digit = (unsigned)(c - '0');
        if (!decimal) {
            if (integer > 99999u) return false;
            integer = integer * 10u + digit;
        } else {
            if (++fractional_digits > 2u) return false;
            fraction = fraction * 10u + digit;
            multiplier *= 10u;
        }
        any = true;
    }
    if (!any || (decimal && fractional_digits == 0u)) return false;
    if (decimal) {
        if (integer > 100u) return false;
        *mv = integer * 1000u + fraction * (1000u / multiplier);
    } else {
        *mv = (integer <= 100u) ? integer * 1000u : integer;
    }
    return true;
}
#endif
