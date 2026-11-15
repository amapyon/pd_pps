#include "m4_ui.h"
#include "pps_apdo_limits.h"
#include <string.h>

static bool editing(m4_page_t page)
{
    return page == M4_PAGE_EDIT_V || page == M4_PAGE_EDIT_I ||
           page == M4_PAGE_CONFIRM;
}

void m4_ui_init(m4_ui_t *ui, uint32_t desired_mv, uint32_t desired_ma)
{
    memset(ui, 0, sizeof(*ui));
    ui->page = M4_PAGE_MONITOR;
    ui->desired_mv = desired_mv;
    ui->desired_ma = desired_ma;
    ui->result = "";
}

void m4_ui_result(m4_ui_t *ui, const char *message)
{
    ui->result = message;
    ui->page = M4_PAGE_RESULT;
    ++ui->revision;
}

void m4_ui_set_pd(m4_ui_t *ui, uint32_t desired_mv, uint32_t desired_ma,
                  bool pd_ready)
{
    if (editing(ui->page)) {
        if (!pd_ready) {
            m4_ui_result(ui, "PD NOT READY");
        } else if (ui->base_mv != desired_mv || ui->base_ma != desired_ma) {
            m4_ui_result(ui, "TARGET CHANGED");
        }
    }
    if (ui->pd_ready != pd_ready) ++ui->revision;
    ui->pd_ready = pd_ready;
    ui->desired_mv = desired_mv;
    ui->desired_ma = desired_ma;
}

static bool same_edit_apdo(const m4_pdo_t *a, const m4_pdo_t *b)
{
    return a->kind == M4_PDO_PPS && b->kind == M4_PDO_PPS &&
           a->source_index == b->source_index &&
           a->min_mv == b->min_mv && a->max_mv == b->max_mv &&
           a->max_ma == b->max_ma;
}

void m4_ui_set_pdos(m4_ui_t *ui, const m4_pdo_t *pdos, uint8_t count)
{
    if (!pdos) count = 0u;
    if (count > M4_MAX_PDOS) count = M4_MAX_PDOS;
    if (ui->pdo_count != count) ++ui->revision;
    ui->pdo_count = count;
    if (count) memcpy(ui->pdos, pdos, sizeof(ui->pdos[0]) * count);
    if (ui->pdo_index >= count) ui->pdo_index = 0u;
    /* Source Capabilities may change asynchronously, even during editing.
     * Never silently reuse a now-different APDO for an armed request. */
    if (editing(ui->page)) {
        bool found_same = false;
        for (uint8_t i = 0u; i < count; ++i) {
            if (same_edit_apdo(&ui->edit_apdo, &ui->pdos[i])) {
                found_same = true;
                break;
            }
        }
        if (!found_same) m4_ui_result(ui, "SOURCE CAPS CHANGED");
    }
}

static bool open_editor(m4_ui_t *ui)
{
    if (!ui->pd_ready) {
        m4_ui_result(ui, "PD NOT READY");
        return false;
    }
    for (uint8_t i = 0u; i < ui->pdo_count; ++i) {
        const m4_pdo_t *p = &ui->pdos[i];
        if (p->kind != M4_PDO_PPS) continue;
        if (!pps_apdo_accepts((PPSApdoLimits){p->min_mv, p->max_mv, p->max_ma},
                              ui->desired_mv, ui->desired_ma)) continue;
        ui->base_mv = ui->desired_mv;
        ui->base_ma = ui->desired_ma;
        ui->edit_mv = ui->desired_mv;
        ui->edit_ma = ui->desired_ma;
        ui->edit_pdo_index = i;
        ui->edit_apdo = *p;
        ui->page = M4_PAGE_EDIT_V;
        ++ui->revision;
        return true;
    }
    m4_ui_result(ui, "NO MATCHING APDO");
    return false;
}

static uint32_t bounded_step(uint32_t v, int8_t dir, uint32_t step,
                             uint32_t min, uint32_t max)
{
    if (dir > 0) return (v + step <= max && v <= max - step) ? v + step : v;
    if (dir < 0) return (v >= min + step) ? v - step : v;
    return v;
}

m4_action_t m4_ui_input(m4_ui_t *ui, m3_events_t events)
{
    m4_action_t action = {0};
    if (events.back_press) {
        switch (ui->page) {
            case M4_PAGE_MONITOR: break;
            case M4_PAGE_MENU: ui->page = M4_PAGE_MONITOR; break;
            case M4_PAGE_CAPS: ui->page = M4_PAGE_MENU; break;
            case M4_PAGE_EDIT_V: ui->page = M4_PAGE_MENU; break;
            case M4_PAGE_EDIT_I: ui->page = M4_PAGE_EDIT_V; break;
            case M4_PAGE_CONFIRM: ui->page = M4_PAGE_EDIT_I; break;
            case M4_PAGE_RESULT: ui->page = M4_PAGE_MENU; break;
        }
        ++ui->revision;
        return action;  /* BACK takes priority over rotation/press. */
    }
    if (events.rotation) {
        if (ui->page == M4_PAGE_MENU) {
            ui->menu_index = (uint8_t)((ui->menu_index +
                (events.rotation > 0 ? 1u : 2u)) % 3u);
        } else if (ui->page == M4_PAGE_CAPS && ui->pdo_count) {
            ui->pdo_index = (uint8_t)((ui->pdo_index +
                (events.rotation > 0 ? 1u : ui->pdo_count - 1u)) % ui->pdo_count);
        } else if (ui->page == M4_PAGE_EDIT_V) {
            const m4_pdo_t *p = &ui->edit_apdo;
            ui->edit_mv = bounded_step(ui->edit_mv, events.rotation,
                                       M4_V_STEP_MV, p->min_mv, p->max_mv);
        } else if (ui->page == M4_PAGE_EDIT_I) {
            const m4_pdo_t *p = &ui->edit_apdo;
            ui->edit_ma = bounded_step(ui->edit_ma, events.rotation,
                                       M4_I_STEP_MA, 50u, p->max_ma);
        }
        ++ui->revision;
    }
    if (!events.encoder_press) return action;
    switch (ui->page) {
        case M4_PAGE_MONITOR:
            ui->page = M4_PAGE_MENU;
            break;
        case M4_PAGE_MENU:
            if (ui->menu_index == 0u) ui->page = M4_PAGE_MONITOR;
            else if (ui->menu_index == 1u) {
                if (ui->pdo_count) ui->page = M4_PAGE_CAPS;
                else m4_ui_result(ui, "NO SOURCE CAPS");
            } else (void)open_editor(ui);
            break;
        case M4_PAGE_CAPS:
            ui->page = M4_PAGE_MENU;
            break;
        case M4_PAGE_EDIT_V:
            ui->page = M4_PAGE_EDIT_I;
            break;
        case M4_PAGE_EDIT_I:
            ui->page = M4_PAGE_CONFIRM;
            break;
        case M4_PAGE_CONFIRM:
            action.apply = true; /* Main firmware MUST check APDO & PD state. */
            action.mv = ui->edit_mv;
            action.ma = ui->edit_ma;
            action.original_mv = ui->base_mv;
            action.original_ma = ui->base_ma;
            action.source_index = ui->edit_apdo.source_index;
            action.apdo_min_mv = ui->edit_apdo.min_mv;
            action.apdo_max_mv = ui->edit_apdo.max_mv;
            action.apdo_max_ma = ui->edit_apdo.max_ma;
            /* Prevent repeat APPLY events on a held/duplicated press. */
            m4_ui_result(ui, "CONFIRM RECEIVED");
            break;
        case M4_PAGE_RESULT:
            ui->page = M4_PAGE_MONITOR;
            break;
    }
    ++ui->revision;
    return action;
}
