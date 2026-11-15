#ifndef M32_DIAG_H
#define M32_DIAG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Diagnostic only: no USB/PD register writes. */
void m32_diag_init(uint32_t now);
void m32_diag_tick(uint32_t now, bool attached, uint8_t pe);
void m32_diag_usb_irq(void);
void m32_diag_bus_reset_irq(void);
size_t m32_diag_reply(const uint8_t *cmd, size_t len, char out[64]);
#endif
