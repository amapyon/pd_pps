#include "m4_screen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool nonblank(const m2_ssd1306_t *oled)
{
    for (unsigned i = 0u; i < M2_OLED_BUFFER_BYTES; ++i)
        if (oled->fb[i]) return true;
    return false;
}
int main(void)
{
    m2_ssd1306_t oled = {0};
    m2_dashboard_values_t meter = {0};
    meter.ina_valid = true;
    meter.bus_mv = 6980u;
    meter.load_ua = -250;
    meter.load_power_mw = -2;
    m4_ui_t ui;
    m4_ui_init(&ui, 7000u, 1000u);
    m4_screen_draw(&oled, &ui, &meter, false);
    assert(nonblank(&oled));
    m4_ui_input(&ui, (m3_events_t){.encoder_press = true});
    m4_screen_draw(&oled, &ui, &meter, false);
    assert(nonblank(&oled));
    assert(ui.page == M4_PAGE_MENU);
    puts("PASS: draw monitor/menu to SSD1306 framebuffer (no MCU)");
    return 0;
}
