#include <stdio.h>
#include "../m7_measurement.h"
int main(void)
{
    M7_Measurement sample = {M7_MEASURE_OK, 99999u, -42};
    M7_ReadMeasurement(&sample);
    if (sample.status != M7_MEASURE_NOT_CONFIGURED) return 1;
    /* Values are not meaningful until status is OK. */
    puts("PASS: device default backend is unavailable (no fabricated readings)");
    return 0;
}
