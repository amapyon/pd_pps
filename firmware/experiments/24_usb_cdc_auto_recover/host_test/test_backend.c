#include <stdio.h>
#include <stdint.h>
#include "../m8_measurement.h"
static int ok;
int M8_INA226_ReadSample(uint32_t *voltage, int32_t *current)
{
    if (!ok) return 0;
    *voltage = 6965;
    *current = 69;
    return 1;
}
int main(void)
{
    M8_Measurement m;
    ok = 0; M8_ReadMeasurement(&m);
    if (m.status != M8_MEASURE_IO_ERROR || m.voltage_mV || m.current_mA) return 1;
    ok = 1; M8_ReadMeasurement(&m);
    if (m.status != M8_MEASURE_OK || m.voltage_mV != 6965 || m.current_mA != 69) return 2;
    puts("PASS: M8 backend failure / valid measurement (mock only)");
    return 0;
}
