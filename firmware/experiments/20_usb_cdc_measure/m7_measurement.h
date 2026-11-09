#ifndef M7_MEASUREMENT_H
#define M7_MEASUREMENT_H
#include <stdint.h>

/* MEASURE reports decimal milli-units. Voltage is never negative;
 * signed current is reserved for future bidirectional sensors.
 * A backend MUST use status=OK only for actual, valid sensor samples.
 */
typedef enum {
    M7_MEASURE_OK = 0,
    M7_MEASURE_NOT_CONFIGURED = 1,
    M7_MEASURE_IO_ERROR = 2
} M7_MeasurementStatus;

typedef struct {
    M7_MeasurementStatus status;
    uint32_t voltage_mV;
    int32_t current_mA;
} M7_Measurement;

void M7_ReadMeasurement(M7_Measurement *out);
#endif
