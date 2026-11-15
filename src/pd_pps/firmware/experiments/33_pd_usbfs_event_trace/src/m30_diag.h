#ifndef M30_DIAG_H
#define M30_DIAG_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
/* Foreground snapshots; no PD writes, prints or I2C transactions. */
void m30_diag_init(uint32_t now);
void m30_diag_tick(uint32_t now, bool attached, uint8_t pe_state);
void m30_diag_usb_irq(void);
void m30_diag_bus_reset_irq(void);
/* Return reply including LF, <=63 bytes, without NUL terminator. */
size_t m30_diag_reply(const uint8_t *command, size_t n, char out[64]);
#endif
