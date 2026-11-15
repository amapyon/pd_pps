#ifndef M35_PHY_H
#define M35_PHY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Diagnostic only. Must NOT connect the PC data cable during a PD run.
 * Preserve all PC14/15 PD pin configuration. */
void m35_phy_init(void);
void m35_phy_tick(uint32_t now, bool pd_attached, uint8_t pe_state);
bool m35_phy_is_off(void);
size_t m35_phy_reply(const uint8_t *cmd, size_t n, char out[64]);
#endif
