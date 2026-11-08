#include "m2_bus_adapter.h"
#include "m2_dashboard.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned writes;
static unsigned data_writes;
static unsigned command_writes;
static uint8_t successful_address = 0x3C;
static uint8_t last_command_page;
static uint8_t last_control_byte;
static unsigned bytes_written;

static bool mock_write(uint8_t addr, const uint8_t *p, size_t len, void *ctx)
{
    (void)ctx;
    assert(len > 0u && len <= 27u); /* init:27 bytes; flush data:17 */
    ++writes;
    if (addr != successful_address) return false;
    bytes_written += (unsigned)len;
    last_control_byte = p[0];
    if (p[0] == 0x00) {
        ++command_writes;
        if (len == 4u) last_command_page = (uint8_t)(p[1] & 0x0Fu);
    } else {
        assert(p[0] == 0x40);
        ++data_writes;
    }
    return true;
}

int main(void)
{
    m2_dashboard_t ui;
    unsigned initial_writes;
    uint32_t i;
    assert(m2_dashboard_init(&ui, mock_write, NULL));
    assert(ui.oled.address7 == 0x3C);
    assert(!ui.oled.busy);
    m2_dashboard_set_pd(&ui, 9000, 1000, true);
    m2_dashboard_set_ina_raw(&ui, 7093, 136, true);
    assert(ui.values.bus_mv == 8866u);
    assert(ui.values.load_ua == 34000);
    assert(ui.values.load_power_mw == 301);
    initial_writes = writes;
    for (i = 0u; i < 160u; ++i) {
        unsigned prev = writes;
        m2_dashboard_service(&ui, i * 2u);
        assert(writes - prev <= 1u); /* one transaction per application tick */
    }
    assert(!ui.oled.busy);
    assert(writes - initial_writes == 72u); /* 8 pages * (1 cmd + 8 x 16 bytes) */
    assert(command_writes == 9u); /* 1 init, 8 page addressing */
    assert(data_writes == 64u);
    assert(bytes_written > 1024u);
    assert(last_command_page == 7u);
    assert(last_control_byte == 0x40u);
    assert(memcmp(ui.oled.fb, "PD/PPS", 6u) != 0); /* bitmap, not raw ASCII */
    initial_writes = writes;
    for (i = 320u; i < 1000u; ++i) m2_dashboard_service(&ui, i);
    assert(writes == initial_writes);
    m2_dashboard_service(&ui, 1000u);
    assert(ui.oled.busy);

    /* Alternate valid OLED address. */
    successful_address = 0x3D;
    assert(m2_dashboard_init(&ui, mock_write, NULL));
    assert(ui.oled.address7 == 0x3D);
    puts("PASS: SSD1306 init, 0x3C/0x3D fallback, raw conversion, chunked frame flush");
    return 0;
}
