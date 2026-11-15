#include "m4_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static m3_events_t rot(int d) { m3_events_t e = {0}; e.rotation = (int8_t)d; return e; }
static m3_events_t push(void) { m3_events_t e = {0}; e.encoder_press = true; return e; }
static m3_events_t back(void) { m3_events_t e = {0}; e.back_press = true; return e; }

int main(void)
{
    m4_ui_t ui;
    m4_ui_init(&ui, 7000u, 1000u);
    assert(ui.page == M4_PAGE_MONITOR);
    const m4_pdo_t caps[] = {
        {M4_PDO_FIXED, 0u, 5000u, 5000u, 3000u},
        {M4_PDO_PPS,   3u, 5000u, 11000u, 1800u}
    };
    m4_ui_set_pdos(&ui, caps, 2u);
    m4_ui_set_pd(&ui, 7000u, 1000u, true);
    assert(!m4_ui_input(&ui, push()).apply);
    assert(ui.page == M4_PAGE_MENU);
    assert(!m4_ui_input(&ui, rot(1)).apply);
    assert(ui.menu_index == 1u);
    assert(!m4_ui_input(&ui, push()).apply);
    assert(ui.page == M4_PAGE_CAPS);
    assert(ui.pdo_count == 2u);
    m4_ui_input(&ui, rot(1));
    assert(ui.pdo_index == 1u && ui.pdos[1].kind == M4_PDO_PPS);
    m4_ui_input(&ui, back());
    assert(ui.page == M4_PAGE_MENU);
    m4_ui_input(&ui, rot(1));
    assert(ui.menu_index == 2u);
    m4_ui_input(&ui, push());
    assert(ui.page == M4_PAGE_EDIT_V && ui.edit_mv == 7000u);
    m4_ui_input(&ui, rot(1));
    assert(ui.edit_mv == 7100u);
    m4_ui_input(&ui, rot(-1));
    assert(ui.edit_mv == 7000u);
    m4_ui_input(&ui, rot(1));
    m4_ui_input(&ui, push());
    assert(ui.page == M4_PAGE_EDIT_I);
    m4_ui_input(&ui, rot(1));
    assert(ui.edit_ma == 1050u);
    m4_ui_input(&ui, push());
    assert(ui.page == M4_PAGE_CONFIRM);
    assert(!m4_ui_input(&ui, (m3_events_t){0}).apply);
    const m4_action_t req = m4_ui_input(&ui, push());
    assert(req.apply && req.mv == 7100u && req.ma == 1050u);
    assert(req.original_mv == 7000u && req.original_ma == 1000u);
    assert(req.source_index == 3u && req.apdo_min_mv == 5000u &&
           req.apdo_max_mv == 11000u && req.apdo_max_ma == 1800u);
    assert(ui.page == M4_PAGE_RESULT);
    assert(!m4_ui_input(&ui, (m3_events_t){0}).apply);
    m4_ui_result(&ui, "DEMO - NO PPS CHANGE");
    m4_ui_input(&ui, push());
    assert(ui.page == M4_PAGE_MONITOR);
    assert(ui.desired_mv == 7000u && ui.desired_ma == 1000u);
    puts("PASS: browse PDO, edit, confirm without affecting committed targets");

    m4_ui_input(&ui, push());
    ui.menu_index = 2u;
    m4_ui_input(&ui, push());
    assert(ui.page == M4_PAGE_EDIT_V);
    m4_ui_input(&ui, back());
    assert(ui.page == M4_PAGE_MENU);
    m4_ui_input(&ui, back());
    assert(ui.page == M4_PAGE_MONITOR);
    puts("PASS: cancel does not change target");

    m4_ui_input(&ui, push());
    ui.menu_index = 2u;
    m4_ui_input(&ui, push());
    m4_ui_set_pd(&ui, 9000u, 1000u, true);
    assert(ui.page == M4_PAGE_RESULT);
    assert(strcmp(ui.result, "TARGET CHANGED") == 0);
    puts("PASS: external console target change cancels draft");

    m4_ui_input(&ui, push());
    m4_ui_input(&ui, push());
    ui.menu_index = 2u;
    m4_ui_set_pd(&ui, 9000u, 1000u, true);
    m4_ui_input(&ui, push());
    assert(ui.page == M4_PAGE_EDIT_V);
    m4_ui_set_pd(&ui, 9000u, 1000u, false);
    assert(ui.page == M4_PAGE_RESULT);
    assert(strcmp(ui.result, "PD NOT READY") == 0);
    puts("PASS: PD disconnect cancels draft");

    m4_ui_init(&ui, 7000u, 1000u);
    m4_ui_input(&ui, push());
    m4_ui_input(&ui, rot(-1));
    assert(ui.menu_index == 2u);
    m4_ui_input(&ui, push());
    assert(ui.page == M4_PAGE_RESULT);
    puts("PASS: editor blocked until valid PD/APDO");

    m4_ui_init(&ui, 7000u, 1000u);
    m4_ui_set_pdos(&ui, caps, 2u);
    m4_ui_set_pd(&ui, 7000u, 1000u, true);
    m4_ui_input(&ui, push());
    ui.menu_index = 2u;
    m4_ui_input(&ui, push());
    assert(ui.page == M4_PAGE_EDIT_V);
    const m4_pdo_t changed_caps[] = {
        {M4_PDO_FIXED, 0u, 5000u, 5000u, 3000u},
        {M4_PDO_PPS,   3u, 5000u, 11000u, 1750u}
    };
    m4_ui_set_pdos(&ui, changed_caps, 2u);
    assert(ui.page == M4_PAGE_RESULT &&
           strcmp(ui.result, "SOURCE CAPS CHANGED") == 0);
    assert(!m4_ui_input(&ui, push()).apply);
    puts("PASS: Source Capabilities change aborts draft");
    return 0;
}
