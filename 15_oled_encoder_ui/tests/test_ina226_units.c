#include "../ina226_units.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(ina226_bus_mv(0) == 0);
    assert(ina226_bus_mv(5600) == 7000); /* 7V */
    assert(ina226_bus_mv(4000) == 5000); /* 5V */
    assert(ina226_r010_current_ua(0) == 0);
    assert(ina226_r010_current_ua(4000) == 1000000); /* 1A */
    assert(ina226_r010_current_ua(-4000) == -1000000);
    assert(ina226_r010_current_ua(32767) == 8191750);
    assert(ina226_power_mw(7000, 1000000) == 7000);
    assert(ina226_power_mw(7000, -1000000) == -7000);
    assert(ina226_power_mw(5000, 250) == 1);
    puts("INA226 R010 scale/power tests OK");
    return 0;
}
