#ifndef M34_DIAG_H
#define M34_DIAG_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
void m34_diag_init(uint32_t now);
void m34_diag_usb_irq(uint16_t intfgst);
void m34_diag_tick(uint32_t now, bool attached, uint8_t pe);
size_t m34_diag_reply(const uint8_t* cmd, size_t len, char out[64]);
#endif
