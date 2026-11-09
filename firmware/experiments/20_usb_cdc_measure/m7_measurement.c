/* Default backend: NO measurement sensor is wired or configured.
 * Never claim zero volts/amperes as a measured value.
 * Replace this implementation with a real hardware driver only after
 * wiring, electrical limits and calibration have been verified.
 */
#include "m7_measurement.h"
void M7_ReadMeasurement(M7_Measurement *out)
{
    if (!out) return;
    out->status = M7_MEASURE_NOT_CONFIGURED;
    out->voltage_mV = 0u;  /* ignored when status is unavailable */
    out->current_mA = 0;   /* ignored when status is unavailable */
}
