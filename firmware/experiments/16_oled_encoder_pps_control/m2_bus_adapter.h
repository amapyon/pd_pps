#ifndef M2_BUS_ADAPTER_H
#define M2_BUS_ADAPTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Adapter for the EXISTING, proven PA2/PA3 software-I2C routines.
 * Do not rewrite their GPIO input/release behavior. */
typedef struct {
    bool (*start)(void *user);                    /* issue START; true if successful */
    bool (*write_byte)(uint8_t byte, void *user);  /* true = ACK */
    void (*stop)(void *user);                     /* always releases bus */
    void *user;
} m2_i2c_ops_t;

bool m2_bus_write(uint8_t address7, const uint8_t *bytes,
                  size_t length, void *context);

#endif
