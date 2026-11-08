#ifndef INA226_H
#define INA226_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t address;       /* 7-bit address, scanned in 0x40..0x4f */
    uint16_t bus_raw;
    int16_t shunt_raw;
    uint32_t bus_mv;
    int32_t current_ua;    /* + current from IN+ toward IN- */
    int32_t power_mw;
} INA226Reading;

/* GPIO bit-bang, SCL=PA2 (QFN20 pin 4), SDA=PA3 (QFN20 pin 5).
 * External pull-ups to 3.3V are required (check module first).
 * CH32X035 cannot use GPIO_CNF_OUT_OD for I2C bit-banging: emulate via
 * floating input (released/high) and push-pull output low (asserted/low).
 * Both lines must be idle high. Not for use in interrupt handlers.
 */
void ina226_bus_init(void);
bool ina226_read(INA226Reading *reading);
/* Diagnostic: ACK scan 0x40..0x4f, IDs, CONFIG write/readback; logs via debugprintf.
 * Call in main-loop command context only; does not change USB PD/PPS settings.
 */
void ina226_scan(void);

/* M2: Existing PA2/PA3 software-I2C bus adapter.
 * These are thin wrappers around the VERIFIED i2c_start/write_byte/stop.
 * Call in the main loop only; the I2C transaction must be ended with end(),
 * including when start/send fails. A true send() result means ACK.
 * No changes to GPIO input/release behavior or INA226 calibration.
 */
bool ina226_i2c_begin(void);
bool ina226_i2c_send(uint8_t byte);
void ina226_i2c_end(void);

#endif
