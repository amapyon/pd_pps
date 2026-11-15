#ifndef M8_MEASUREMENT_H
#define M8_MEASUREMENT_H
#include <stdint.h>

/* MEASURE reports decimal milli-units. Voltage is never negative;
 * signed current is reserved for future bidirectional sensors.
 * A backend MUST use status=OK only for actual, valid sensor samples.
 */
typedef enum {
    M8_MEASURE_OK = 0,
    M8_MEASURE_NOT_CONFIGURED = 1,
    M8_MEASURE_IO_ERROR = 2
} M8_MeasurementStatus;

typedef struct {
    M8_MeasurementStatus status;
    uint32_t voltage_mV;
    int32_t current_mA;
} M8_Measurement;

void M8_ReadMeasurement(M8_Measurement *out);
#endif
