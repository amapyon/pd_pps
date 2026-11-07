/* M13f: exercise the ACTUAL Policy Engine at millisecond boundaries.
 * No real charger or MCU, and no delay/sleep-based timing assumptions. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "pd_policy.h"

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
    return false; } } while (0)

static const pe_caps_t pps_caps = {
    .pdo={0x0801912cu,0x0002d12cu,0x0003c12cu,0x0004b12cu,
          0x000641f4u,0xc1a4213cu},.count=6u,.msg_id=0u
};
static void begin_pps(pe_t *p, uint32_t ready) {
    pe_init(p);
    pe_event(p, PE_EV_ATTACH, NULL, ready - 30u);
    pe_event(p, PE_EV_CAPS, &pps_caps, ready - 10u);
    pe_command_result(p, true, ready - 9u);
    pe_event(p, PE_EV_ACCEPT, NULL, ready - 5u);
    pe_event(p, PE_EV_PS_RDY, NULL, ready);
}
static bool is_pps_ready(const pe_t *p) {
    return p->state == PE_READY && p->contract_valid && p->pps_active &&
           p->command == PE_CMD_NONE && p->rdo == 0x6302bc14u;
}
static void complete_refresh(pe_t *p, uint32_t start) {
    pe_command_result(p, true, start);
    pe_event(p, PE_EV_ACCEPT, NULL, start + 1u);
    pe_event(p, PE_EV_PS_RDY, NULL, start + 3u);
}
static bool refresh_exact_4999_5000(void) {
    pe_t p; begin_pps(&p, 100u);
    CHECK(is_pps_ready(&p));
    pe_tick(&p, 5099u);
    CHECK(is_pps_ready(&p));
    pe_tick(&p, 5100u);
    CHECK(p.state == PE_WAIT_ACCEPT && p.command == PE_CMD_REQUEST);
    CHECK(p.contract_valid && p.rdo == 0x6302bc14u);
    complete_refresh(&p, 5100u);
    CHECK(is_pps_ready(&p) && p.last_pps_ms == 5103u);
    pe_tick(&p, 10102u); CHECK(is_pps_ready(&p));
    pe_tick(&p, 10103u);
    CHECK(p.state == PE_WAIT_ACCEPT && p.command == PE_CMD_REQUEST);
    return true;
}
static bool high_uptime_no_wait_deadline(void) {
    /* Original defect: reached(now, 0) rejects high-half 32-bit uptime. */
    pe_t p; const uint32_t ready = 0xffffe000u;
    begin_pps(&p, ready); CHECK(is_pps_ready(&p));
    CHECK(!p.defer_active);
    pe_tick(&p, ready + 4999u); CHECK(is_pps_ready(&p));
    pe_tick(&p, ready + 5000u);
    CHECK(p.state == PE_WAIT_ACCEPT && p.command == PE_CMD_REQUEST);
    return true;
}
static bool refresh_across_ms_wrap(void) {
    pe_t p; const uint32_t ready = 0xfffffff0u;
    begin_pps(&p, ready); CHECK(is_pps_ready(&p));
    pe_tick(&p, ready + 4999u); CHECK(is_pps_ready(&p));
    pe_tick(&p, ready + 5000u);
    CHECK(p.state == PE_WAIT_ACCEPT && p.command == PE_CMD_REQUEST);
    complete_refresh(&p, ready + 5000u);
    CHECK(is_pps_ready(&p));
    return true;
}
static bool wait_backoff_exact_119_120(void) {
    pe_t p; begin_pps(&p, 20u);
    pe_tick(&p, 5020u);
    CHECK(p.command == PE_CMD_REQUEST);
    pe_command_result(&p, true, 5020u);
    pe_event(&p, PE_EV_WAIT, NULL, 5022u);
    CHECK(is_pps_ready(&p));
    CHECK(p.defer_active && p.defer_until_ms == 5142u);
    pe_tick(&p, 5141u);
    CHECK(is_pps_ready(&p) && p.defer_active);
    pe_tick(&p, 5142u);
    CHECK(!p.defer_active && p.command == PE_CMD_REQUEST && p.state == PE_WAIT_ACCEPT);
    return true;
}
static bool wait_backoff_across_wrap(void) {
    pe_t p; const uint32_t ready = 0xffffe000u;
    begin_pps(&p, ready);
    uint32_t attempt = ready + 5000u;
    pe_tick(&p, attempt); CHECK(p.command == PE_CMD_REQUEST);
    pe_command_result(&p, true, attempt);
    pe_event(&p, PE_EV_WAIT, NULL, attempt + 2u);
    CHECK(p.defer_active);
    pe_tick(&p, attempt + 121u);
    CHECK(p.state == PE_READY && p.defer_active);
    pe_tick(&p, attempt + 122u);
    CHECK(p.state == PE_WAIT_ACCEPT && !p.defer_active);
    return true;
}
static bool wait_deadline_wrap_itself(void) {
    pe_t p; const uint32_t ready = 0xffffec55u;
    begin_pps(&p, ready);
    uint32_t attempt = ready + 5000u;
    /* Request starts 19ms before WAIT; the WAIT deadline crosses wrap. */
    pe_tick(&p, attempt); CHECK(p.command == PE_CMD_REQUEST);
    pe_command_result(&p, true, attempt);
    const uint32_t wait_at = 0xfffffff0u;
    /* A response shortly after sending the Request is valid. */
    pe_event(&p, PE_EV_WAIT, NULL, wait_at);
    CHECK(p.defer_active && p.defer_until_ms == (uint32_t)(wait_at + 120u));
    pe_tick(&p, wait_at + 119u); CHECK(p.state == PE_READY);
    pe_tick(&p, wait_at + 120u); CHECK(p.command == PE_CMD_REQUEST);
    return true;
}
static bool expired_backoff_never_reactivates(void) {
    pe_t p; begin_pps(&p, 15u);
    pe_tick(&p, 5015u); pe_command_result(&p, true, 5015u);
    pe_event(&p, PE_EV_WAIT, NULL, 5016u);
    pe_tick(&p, 5136u);
    CHECK(!p.defer_active && p.command == PE_CMD_REQUEST);
    complete_refresh(&p, 5136u);
    CHECK(is_pps_ready(&p));
    uint32_t due = p.last_pps_ms + 5000u;
    pe_tick(&p, due);
    CHECK(p.command == PE_CMD_REQUEST && !p.defer_active);
    return true;
}
static bool eleven_refresh_cycles_cross_sign_boundary(void) {
    pe_t p; const uint32_t start = 0x7fffff00u;
    begin_pps(&p, start);
    for (unsigned i=0; i<11u; ++i) {
        uint32_t due = p.last_pps_ms + 5000u;
        pe_tick(&p, due-1u);
        CHECK(is_pps_ready(&p));
        pe_tick(&p, due);
        CHECK(p.command == PE_CMD_REQUEST && p.rdo == 0x6302bc14u);
        complete_refresh(&p, due);
        CHECK(is_pps_ready(&p));
    }
    return true;
}
static bool caps_timeout_exact_edge_wrap(void) {
    pe_t p; const uint32_t attach = 0xffffff00u;
    pe_init(&p); pe_event(&p, PE_EV_ATTACH, NULL, attach);
    pe_tick(&p, attach+649u);
    CHECK(p.state == PE_WAIT_CAPS && p.faults == 0u);
    pe_tick(&p, attach+650u);
    CHECK(p.state == PE_RECOVERY && p.faults == 1u);
    return true;
}
static bool request_start_limit_exact_edge(void) {
    pe_t p; const uint32_t caps_time = 0xffffffe0u;
    pe_init(&p); pe_event(&p, PE_EV_ATTACH, NULL, caps_time-20u);
    pe_event(&p, PE_EV_CAPS, &pps_caps, caps_time);
    pe_tick(&p, caps_time + 14u);
    CHECK(p.state == PE_WAIT_ACCEPT && p.command == PE_CMD_REQUEST);
    pe_tick(&p, caps_time + 15u);
    CHECK(p.state == PE_RECOVERY && p.faults == 1u);
    return true;
}
static bool accept_timeout_exact_edge(void) {
    pe_t p; const uint32_t sent = 0xfffffff0u;
    pe_init(&p); pe_event(&p, PE_EV_ATTACH, NULL, sent-25u);
    pe_event(&p, PE_EV_CAPS, &pps_caps, sent-1u);
    pe_command_result(&p, true, sent);
    pe_tick(&p, sent + 44u);
    CHECK(p.state == PE_WAIT_ACCEPT && p.faults == 0u);
    pe_tick(&p, sent + 45u);
    CHECK(p.state == PE_RECOVERY && p.faults == 1u);
    return true;
}
static bool psrdy_timeout_exact_edge(void) {
    pe_t p; const uint32_t accept_at = 0xfffffff0u;
    pe_init(&p); pe_event(&p, PE_EV_ATTACH, NULL, accept_at-25u);
    pe_event(&p, PE_EV_CAPS, &pps_caps, accept_at-2u);
    pe_command_result(&p, true, accept_at-1u);
    pe_event(&p, PE_EV_ACCEPT, NULL, accept_at);
    pe_tick(&p, accept_at+499u);
    CHECK(p.state == PE_WAIT_PS_RDY && p.faults == 0u);
    pe_tick(&p, accept_at+500u);
    CHECK(p.state == PE_RECOVERY && p.faults == 1u);
    return true;
}
static bool initial_wait_cannot_bypass_caps_timeout(void) {
    pe_t p;
    pe_init(&p);
    pe_event(&p, PE_EV_ATTACH, NULL, 0u);
    pe_event(&p, PE_EV_CAPS, &pps_caps, 10u);
    pe_command_result(&p, true, 11u);
    pe_event(&p, PE_EV_WAIT, NULL, 12u);
    CHECK(p.state == PE_WAIT_CAPS && !p.contract_valid && p.defer_active);
    pe_tick(&p, 661u);
    CHECK(p.state == PE_WAIT_CAPS && p.faults == 0u);
    pe_tick(&p, 662u);
    CHECK(p.state == PE_RECOVERY && !p.contract_valid && p.faults == 1u);
    CHECK(!p.defer_active);
    return true;
}
static bool fixed5_never_pps_refreshes(void) {
    pe_t p; const uint32_t at = 0xffffe000u;
    pe_caps_t fixed = {.pdo={0x0801912cu},.count=1u};
    pe_init(&p); pe_event(&p, PE_EV_ATTACH, NULL, at-30u);
    pe_event(&p, PE_EV_CAPS, &fixed, at-10u);
    pe_command_result(&p, true, at-9u);
    pe_event(&p, PE_EV_ACCEPT, NULL, at-5u);
    pe_event(&p, PE_EV_PS_RDY, NULL, at);
    CHECK(p.state == PE_READY && !p.pps_active && p.contract_valid);
    pe_tick(&p, at+100000u);
    CHECK(p.state == PE_READY && p.command == PE_CMD_NONE);
    return true;
}
static bool detach_clears_pending_backoff(void) {
    pe_t p; begin_pps(&p, 10u);
    pe_tick(&p, 5010u); pe_command_result(&p, true, 5010u);
    pe_event(&p, PE_EV_WAIT, NULL, 5011u);
    CHECK(p.defer_active);
    pe_event(&p, PE_EV_DETACH, NULL, 5012u);
    CHECK(p.state == PE_DETACHED && !p.defer_active);
    begin_pps(&p, 0xffffe000u);
    pe_tick(&p, 0xffffe000u+5000u);
    CHECK(p.command == PE_CMD_REQUEST);
    return true;
}

typedef bool (*case_fn)(void);
static struct { const char *name; case_fn fn; } cases[] = {
    {"PPS 4999/5000ms and next PS_RDY anchor",refresh_exact_4999_5000},
    {"32bit high uptime / no WAIT sentinel",high_uptime_no_wait_deadline},
    {"PPS refresh across 32bit ms wrap",refresh_across_ms_wrap},
    {"WAIT 119/120ms exact boundary",wait_backoff_exact_119_120},
    {"WAIT after PPS high uptime",wait_backoff_across_wrap},
    {"WAIT deadline across actual wrap",wait_deadline_wrap_itself},
    {"expired WAIT never blocks later refresh",expired_backoff_never_reactivates},
    {"eleven PPS renewals cross signed half-range",eleven_refresh_cycles_cross_sign_boundary},
    {"SourceCaps 649/650ms wrap",caps_timeout_exact_edge_wrap},
    {"Request start 14/15ms wrap",request_start_limit_exact_edge},
    {"Accept 44/45ms wrap",accept_timeout_exact_edge},
    {"PS_RDY 499/500ms wrap",psrdy_timeout_exact_edge},
    {"initial WAIT cannot bypass Caps timeout",initial_wait_cannot_bypass_caps_timeout},
    {"fixed 5V never PPS renews",fixed5_never_pps_refreshes},
    {"CC detach clears WAIT backoff",detach_clears_pending_backoff},
};
int main(void) {
    for (unsigned i=0u; i<sizeof(cases)/sizeof(cases[0]); ++i) {
        if (!cases[i].fn()) return 1;
        printf("PASS %02u %s\n",i+1u,cases[i].name);
    }
    printf("M13f Policy timing: %zu/%zu PASS\n",
           sizeof(cases)/sizeof(cases[0]),sizeof(cases)/sizeof(cases[0]));
    return 0;
}
