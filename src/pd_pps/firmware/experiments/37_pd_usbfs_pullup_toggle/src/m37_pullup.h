#ifndef M37_PULLUP_H
#define M37_PULLUP_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
/* Diagnostic-only: never connect PC USB cable during PD test. */
void m37_pullup_init(void);
void m37_pullup_tick(uint32_t now, bool pd_attached, uint8_t pe_state);
bool m37_pullup_usb_service_blocked(void);
uint32_t m37_pullup_led_interval_ms(void);
size_t m37_pullup_reply(const uint8_t *cmd,size_t len,char out[64]);
#endif
