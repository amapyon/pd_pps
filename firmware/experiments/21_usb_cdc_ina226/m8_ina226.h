#ifndef M8_INA226_H
#define M8_INA226_H
#include <stdint.h>
/* Returns 1 ONLY when the real INA226 read succeeded and IDs match.
 * SCL=PA2, SDA=PA3, 0x40, shunt=10 milliohms (R010).
 */
int M8_INA226_ReadSample(uint32_t *bus_mV, int32_t *current_mA);
/* Exposed pure conversion for host regression testing. */
uint32_t M8_INA226_BusRawToMilliVolts(uint16_t raw);
int32_t M8_INA226_ShuntRawToMilliAmps(int16_t raw);
#endif
