#ifndef M4_SCREEN_H
#define M4_SCREEN_H

#include "m4_ui.h"
#include "m2_dashboard.h"
#include <stdint.h>

/* Draw at most 8 x 21 uppercase ASCII character cells on the M2 framebuffer.
 * Call ONLY when OLED is idle (a transfer must not be in progress). */
void m4_screen_draw(m2_ssd1306_t *oled, const m4_ui_t *ui,
                    const m2_dashboard_values_t *meter,
                    bool apply_enabled);

#endif
