/* M8: real INA226 measurement backend. No fabricated target data. */
#include "m8_measurement.h"
#include "m8_ina226.h"
void M8_ReadMeasurement(M8_Measurement *out)
{
    if (!out) return;
    out->voltage_mV = 0u;
    out->current_mA = 0;
    out->status = M8_MEASURE_IO_ERROR;
    uint32_t mv;
    int32_t ma;
    if (M8_INA226_ReadSample(&mv, &ma)) {
        out->voltage_mV = mv;
        out->current_mA = ma;
        out->status = M8_MEASURE_OK;
    }
}
