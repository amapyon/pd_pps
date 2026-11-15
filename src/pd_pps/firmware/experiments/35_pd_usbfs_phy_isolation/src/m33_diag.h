#ifndef M33_DIAG_H
#define M33_DIAG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
void m33_diag_init(uint32_t now);
void m33_diag_tick(uint32_t now, bool attached, uint8_t pe);
void m33_diag_usb_irq(uint16_t intfgst);
void m33_diag_set_config_irq(uint8_t cfg);
size_t m33_diag_reply(const uint8_t *cmd,size_t len,char out[64]);
#endif
