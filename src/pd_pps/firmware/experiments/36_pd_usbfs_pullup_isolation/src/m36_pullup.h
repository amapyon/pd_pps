#ifndef M36_PULLUP_H
#define M36_PULLUP_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
/* M36 is a hardware diagnostic, NOT normal USB CDC operation.
 * Never connect the PC USB data cable while PD is attached. */
void m36_pullup_init(void);
void m36_pullup_tick(uint32_t now, bool pd_attached, uint8_t pe_state);
bool m36_pullup_usb_service_blocked(void);
uint32_t m36_pullup_led_interval_ms(void);
size_t m36_pullup_reply(const uint8_t *cmd,size_t len,char out[64]);
#endif
