#include "m2_dashboard.h"
#include "ina226_units.h"
#include <stdio.h>
#include <string.h>

/* Converts signed thousandths to a fixed width decimal string (no float printf). */
static void fixed_decimal(char *dst, size_t cap, int32_t number,
                          uint32_t unit, uint32_t tenth_unit, unsigned decimals)
{
    uint32_t absolute;
    uint32_t whole;
    uint32_t fraction;
    const char *sign;
    if (!cap) return;
    absolute = (number < 0) ? (uint32_t)(-(int64_t)number) : (uint32_t)number;
    sign = (number < 0) ? "-" : "";
    absolute += tenth_unit / 2u; /* rounding before decimal scaling */
    whole = absolute / unit;
    fraction = (absolute % unit) / tenth_unit;
    if (decimals == 3u) {
        (void)snprintf(dst, cap, "%s%lu.%03lu", sign, (unsigned long)whole,
                       (unsigned long)fraction);
    } else {
        (void)snprintf(dst, cap, "%s%lu.%02lu", sign, (unsigned long)whole,
                       (unsigned long)fraction);
    }
}

static void render(m2_dashboard_t *ui)
{
    char decimal[24];
    char line[64];
    m2_ssd1306_t *oled = &ui->oled;
    m2_ssd1306_clear(oled);
    m2_ssd1306_text(oled, 0u, 0u, "PD/PPS EXPLORER");
    fixed_decimal(decimal, sizeof(decimal), ui->values.requested_mv,
                  1000u, 10u, 2u);
    (void)snprintf(line, sizeof(line), "PPS %sV", decimal);
    m2_ssd1306_text(oled, 1u, 0u, line);
    (void)snprintf(line, sizeof(line), "REQ %uMA (PD)", (unsigned)ui->values.requested_ma);
    m2_ssd1306_text(oled, 2u, 0u, line);
    if (ui->values.ina_valid) {
        fixed_decimal(decimal, sizeof(decimal), (int32_t)ui->values.bus_mv,
                      1000u, 10u, 2u);
        (void)snprintf(line, sizeof(line), "BUS %sV", decimal);
        m2_ssd1306_text(oled, 3u, 0u, line);
        fixed_decimal(decimal, sizeof(decimal), ui->values.load_ua,
                      1000u, 10u, 2u);
        (void)snprintf(line, sizeof(line), "LOAD %sMA", decimal);
        m2_ssd1306_text(oled, 4u, 0u, line);
        fixed_decimal(decimal, sizeof(decimal), ui->values.load_power_mw,
                      1000u, 1u, 3u);
        (void)snprintf(line, sizeof(line), "PWR %sW", decimal);
        m2_ssd1306_text(oled, 5u, 0u, line);
    } else {
        m2_ssd1306_text(oled, 3u, 0u, "BUS NO DATA");
        m2_ssd1306_text(oled, 4u, 0u, "LOAD NO DATA");
        m2_ssd1306_text(oled, 5u, 0u, "PWR NO DATA");
    }
    (void)snprintf(line, sizeof(line), "PD %s INA %s",
                   ui->values.pd_ready ? "READY" : "WAIT",
                   ui->values.ina_valid ? "OK" : "ERR");
    m2_ssd1306_text(oled, 6u, 0u, line);
    m2_ssd1306_text(oled, 7u, 0u, "REQ != LOAD CURR");
}

bool m2_dashboard_init(m2_dashboard_t *ui, m2_i2c_write_fn write,
                       void *context)
{
    if (!ui) return false;
    memset(ui, 0, sizeof(*ui));
    if (!m2_ssd1306_init_auto(&ui->oled, write, context)) {
        ui->display_fault = true;
        return false;
    }
    return true;
}

void m2_dashboard_set_pd(m2_dashboard_t *ui, int32_t requested_mv,
                         uint16_t requested_ma, bool pd_ready)
{
    if (!ui) return;
    ui->values.requested_mv = requested_mv;
    ui->values.requested_ma = requested_ma;
    ui->values.pd_ready = pd_ready;
}

void m2_dashboard_set_ina_raw(m2_dashboard_t *ui, uint16_t bus_raw,
                              int16_t shunt_raw, bool valid)
{
    if (!ui) return;
    ui->values.ina_valid = valid;
    if (!valid) return;
    /* INA226: BUS=1.25mV/LSB; shunt=2.5uV/LSB, Rshunt=10mOhm. */
    ui->values.bus_mv = ina226_bus_mv(bus_raw);
    ui->values.load_ua = ina226_r010_current_ua(shunt_raw);
    ui->values.load_power_mw = ina226_power_mw(ui->values.bus_mv,
                                               ui->values.load_ua);
}

void m2_dashboard_service(m2_dashboard_t *ui, uint32_t now_ms)
{
    m2_flush_result_t result;
    if (!ui || ui->display_fault) return;
    if (ui->oled.busy) {
        result = m2_ssd1306_flush_step(&ui->oled);
        if (result == M2_FLUSH_ERROR) ui->display_fault = true;
        return;
    }
    if (ui->rendered_once && (uint32_t)(now_ms - ui->last_render_ms) < 1000u)
        return;
    render(ui);
    if (!m2_ssd1306_begin_flush(&ui->oled)) {
        ui->display_fault = true;
        return;
    }
    ui->last_render_ms = now_ms;
    ui->rendered_once = true;
    result = m2_ssd1306_flush_step(&ui->oled);
    if (result == M2_FLUSH_ERROR) ui->display_fault = true;
}
