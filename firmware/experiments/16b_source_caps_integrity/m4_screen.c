#include "m4_screen.h"
#include <stdio.h>

static void row(m2_ssd1306_t *oled, uint8_t n, const char *text)
{
    m2_ssd1306_text(oled, n, 0u, text);
}

static void decimal(char *dst, size_t len, int32_t n, uint32_t unit,
                    uint32_t fraction_unit, unsigned width)
{
    const uint32_t a = (uint32_t)(n < 0 ? -(int64_t)n : (int64_t)n);
    const uint32_t fraction = (a % unit) / fraction_unit;
    if (width == 3u)
        (void)snprintf(dst, len, "%s%lu.%03lu", n < 0 ? "-" : "",
                       (unsigned long)(a / unit), (unsigned long)fraction);
    else
        (void)snprintf(dst, len, "%s%lu.%02lu", n < 0 ? "-" : "",
                       (unsigned long)(a / unit), (unsigned long)fraction);
}

void m4_screen_draw(m2_ssd1306_t *oled, const m4_ui_t *ui,
                    const m2_dashboard_values_t *meter,
                    bool apply_enabled)
{
    char line[40];
    char val[20];
    if (!oled || !ui || !meter) return;
    m2_ssd1306_clear(oled);
    switch (ui->page) {
    case M4_PAGE_MONITOR:
        row(oled, 0, "PD/PPS EXPLORER");
        decimal(val, sizeof(val), (int32_t)ui->desired_mv, 1000u, 10u, 2u);
        (void)snprintf(line, sizeof(line), "PPS %sV", val);
        row(oled, 1, line);
        (void)snprintf(line, sizeof(line), "REQ %luMA (PD)",
                       (unsigned long)ui->desired_ma);
        row(oled, 2, line);
        if (meter->ina_valid) {
            decimal(val, sizeof(val), (int32_t)meter->bus_mv, 1000u, 10u, 2u);
            (void)snprintf(line, sizeof(line), "BUS %sV", val);
            row(oled, 3, line);
            decimal(val, sizeof(val), meter->load_ua, 1000u, 10u, 2u);
            (void)snprintf(line, sizeof(line), "LOAD %sMA", val);
            row(oled, 4, line);
            decimal(val, sizeof(val), meter->load_power_mw, 1000u, 1u, 3u);
            (void)snprintf(line, sizeof(line), "PWR %sW", val);
            row(oled, 5, line);
        } else {
            row(oled, 3, "BUS NO DATA");
            row(oled, 4, "LOAD NO DATA");
            row(oled, 5, "PWR NO DATA");
        }
        (void)snprintf(line, sizeof(line), "PD %s INA %s",
                       ui->pd_ready ? "READY" : "WAIT",
                       meter->ina_valid ? "OK" : "ERR");
        row(oled, 6, line);
        row(oled, 7, "PUSH: MENU");
        break;
    case M4_PAGE_MENU:
        row(oled, 0, "MAIN MENU");
        row(oled, 1, ui->menu_index == 0u ? "> MONITOR" : "  MONITOR");
        row(oled, 2, ui->menu_index == 1u ? "> SOURCE PDO" : "  SOURCE PDO");
        row(oled, 3, ui->menu_index == 2u ? "> PPS EDIT" : "  PPS EDIT");
        row(oled, 5, "ROTATE: SELECT");
        row(oled, 6, "PUSH: ENTER");
        row(oled, 7, "BACK: MONITOR");
        break;
    case M4_PAGE_CAPS:
        row(oled, 0, "SOURCE CAPABILITIES");
        if (ui->pdo_count) {
            const m4_pdo_t *p = &ui->pdos[ui->pdo_index];
            (void)snprintf(line, sizeof(line), "PDO %u/%u", (unsigned)ui->pdo_index + 1u,
                           (unsigned)ui->pdo_count);
            row(oled, 1, line);
            if (p->kind == M4_PDO_PPS) {
                decimal(val, sizeof(val), (int32_t)p->min_mv, 1000u, 10u, 2u);
                (void)snprintf(line, sizeof(line), "PPS MIN %sV", val);
                row(oled, 2, line);
                decimal(val, sizeof(val), (int32_t)p->max_mv, 1000u, 10u, 2u);
                (void)snprintf(line, sizeof(line), "PPS MAX %sV", val);
                row(oled, 3, line);
            } else if (p->kind == M4_PDO_FIXED) {
                decimal(val, sizeof(val), (int32_t)p->min_mv, 1000u, 10u, 2u);
                (void)snprintf(line, sizeof(line), "FIXED %sV", val);
                row(oled, 2, line);
            } else {
                row(oled, 2, "OTHER PDO");
            }
            (void)snprintf(line, sizeof(line), "MAX %luMA", (unsigned long)p->max_ma);
            row(oled, 4, line);
        } else row(oled, 2, "NO SOURCE CAPS");
        row(oled, 6, "ROTATE: NEXT");
        row(oled, 7, "BACK: MENU");
        break;
    case M4_PAGE_EDIT_V: {
        const m4_pdo_t *p = &ui->edit_apdo;
        row(oled, 0, "EDIT PPS VOLTAGE");
        decimal(val, sizeof(val), (int32_t)ui->edit_mv, 1000u, 10u, 2u);
        (void)snprintf(line, sizeof(line), "TARGET %sV", val);
        row(oled, 2, line);
        decimal(val, sizeof(val), (int32_t)p->min_mv, 1000u, 10u, 2u);
        (void)snprintf(line, sizeof(line), "MIN %sV", val);
        row(oled, 3, line);
        decimal(val, sizeof(val), (int32_t)p->max_mv, 1000u, 10u, 2u);
        (void)snprintf(line, sizeof(line), "MAX %sV", val);
        row(oled, 4, line);
        row(oled, 5, "STEP 0.10V");
        row(oled, 6, "PUSH: NEXT");
        row(oled, 7, "BACK: CANCEL");
        break;
    }
    case M4_PAGE_EDIT_I: {
        const m4_pdo_t *p = &ui->edit_apdo;
        row(oled, 0, "EDIT PPS REQ CURRENT");
        (void)snprintf(line, sizeof(line), "REQ %luMA", (unsigned long)ui->edit_ma);
        row(oled, 2, line);
        (void)snprintf(line, sizeof(line), "APDO MAX %luMA", (unsigned long)p->max_ma);
        row(oled, 3, line);
        row(oled, 4, "NOT LOAD CURRENT!");
        row(oled, 5, "STEP 50MA");
        row(oled, 6, "PUSH: CONFIRM");
        row(oled, 7, "BACK: EDIT VOLTAGE");
        break;
    }
    case M4_PAGE_CONFIRM:
        row(oled, 0, "CONFIRM PPS REQUEST");
        decimal(val, sizeof(val), (int32_t)ui->edit_mv, 1000u, 10u, 2u);
        (void)snprintf(line, sizeof(line), "VOLTAGE %sV", val);
        row(oled, 2, line);
        (void)snprintf(line, sizeof(line), "REQ %luMA (PD)",
                       (unsigned long)ui->edit_ma);
        row(oled, 3, line);
        row(oled, 4, "NOT LOAD CURRENT!");
        row(oled, 5, apply_enabled ? "LIVE PPS CHANGE" : "DEMO: NO PPS CHANGE");
        row(oled, 6, apply_enabled ? "PUSH: APPLY" : "PUSH: PREVIEW");
        row(oled, 7, "BACK: EDIT");
        break;
    case M4_PAGE_RESULT:
        row(oled, 0, "PPS UI RESULT");
        row(oled, 2, ui->result ? ui->result : "");
        row(oled, 5, "PUSH: MONITOR");
        row(oled, 7, "BACK: MENU");
        break;
    default: break;
    }
}
