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

#endif
