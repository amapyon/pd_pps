#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "pd_hr_pps_probe.h"

static const pe_caps_t six = {
    .pdo = {0x0801912cu,0x0002d12cu,0x0003c12cu,0x0004b12cu,
            0x0006412cu,0xc1a4213cu}, .count = 6, .msg_id = 0
};
static const pe_caps_t five = {
    .pdo = {0x0801912cu,0x0002d12cu,0x0003c12cu,0x0004b12cu,
            0x0006412cu}, .count = 5, .msg_id = 0
};
static void caps_to_ready(pe_t *p, const pe_caps_t *c, uint32_t now)
{
    pe_event(p, PE_EV_CAPS, c, now);
    assert(p->state == PE_WAIT_ACCEPT && p->command == PE_CMD_REQUEST);
    pe_command_result(p, true, now);
    pe_event(p, PE_EV_ACCEPT, 0, now+1u);
    pe_event(p, PE_EV_PS_RDY, 0, now+3u);
    assert(p->state == PE_READY && p->contract_valid);
}
static void recover_fixed5(pe_t *p)
{
    pe_init(p);
    pe_event(p, PE_EV_ATTACH, NULL, 0);
    caps_to_ready(p, &six, 10);
    assert(p->pps_active);
    pe_event(p, PE_EV_HARD_RESET_TX, NULL, 25);
    assert(p->state == PE_RECOVERY);
    pe_event(p, PE_EV_HR_VBUS_READY, NULL, 940);
    assert(p->state == PE_WAIT_CAPS);
    caps_to_ready(p, &five, 1000);
    assert(!p->pps_active && p->requested_mv == 5000u &&
           p->requested_ma == 1000u);
}
static bool step(pd_hr_pps_probe_t *q, pe_t *p, uint32_t now,
                 bool completed, bool cc, bool blocked)
{
    return pd_hr_pps_probe_step(q, now, completed, cc, blocked, p);
}
static void queue_and_transmit(pd_hr_pps_probe_t *q, pe_t *p, uint32_t now)
{
    assert(pe_request_source_caps(p));
    pd_hr_pps_probe_queued(q, now);
    assert(q->state == PD_HR_PPS_PROBE_TX_QUEUED);
    assert(p->command == PE_CMD_GET_SOURCE_CAP);
    pd_hr_pps_probe_tx_started(q, now);
    pe_command_result(p, true, now);
    assert(q->state == PD_HR_PPS_PROBE_WAIT_CAPS && q->sent);
    assert(p->state == PE_READY && p->command == PE_CMD_NONE);
}
static void test_pps_appears(void)
{
    pe_t p;
    pd_hr_pps_probe_t q;
    recover_fixed5(&p); pd_hr_pps_probe_init(&q);
    assert(!step(&q,&p,1100,true,true,false));
    assert(q.state == PD_HR_PPS_PROBE_DELAY);
    assert(!step(&q,&p,2099,true,true,false));
    assert(step(&q,&p,2100,true,true,false));
    queue_and_transmit(&q,&p,2100);
    pe_caps_t updated=six; updated.msg_id=1;
    pe_event(&p, PE_EV_CAPS, &updated, 2105);
    pd_hr_pps_probe_caps(&q,&p);
    assert(q.caps_seen && q.pps_offered);
    assert(q.state == PD_HR_PPS_PROBE_PPS_OFFERED);
    assert(p.rdo == 0x6302bc14u && p.pending_pps);
    assert(!step(&q,&p,2106,true,true,false));
    pe_command_result(&p,true,2107);
    pe_event(&p,PE_EV_ACCEPT,NULL,2110);
    pe_event(&p,PE_EV_PS_RDY,NULL,2220);
    assert(!step(&q,&p,2221,true,true,false));
    assert(q.state == PD_HR_PPS_PROBE_PPS_READY && q.pps_ready);
    assert(p.pps_active && p.requested_mv == 7000u);
    assert(!step(&q,&p,100000,true,true,false));
    assert(q.sent && q.caps_seen && !q.timeout);
}
static void test_fixed_only(void)
{
    pe_t p; pd_hr_pps_probe_t q;
    recover_fixed5(&p); pd_hr_pps_probe_init(&q);
    assert(!step(&q,&p,2000,true,true,false));
    assert(step(&q,&p,3000,true,true,false));
    queue_and_transmit(&q,&p,3000);
    pe_caps_t updated=five; updated.msg_id=1;
    pe_event(&p,PE_EV_CAPS,&updated,3002);
    pd_hr_pps_probe_caps(&q,&p);
    assert(q.state == PD_HR_PPS_PROBE_FIXED_ONLY);
    assert(q.caps_seen && !q.pps_offered);
    /* In SPR, real SourceCaps are evaluated and answered with one Request,
       even when the fixed-5V contract is simply reaffirmed. */
    assert(p.command == PE_CMD_REQUEST && p.rdo == 0x13019064u);
    pe_command_result(&p,true,3003);
    pe_event(&p,PE_EV_ACCEPT,NULL,3004);
    pe_event(&p,PE_EV_PS_RDY,NULL,3050);
    assert(p.state == PE_READY && !p.pps_active && p.requested_mv == 5000u);
    assert(!step(&q,&p,40000,true,true,false));
    assert(q.state == PD_HR_PPS_PROBE_FIXED_ONLY);
    assert(p.command == PE_CMD_NONE);
}
static void test_no_response(void)
{
    pe_t p; pd_hr_pps_probe_t q;
    recover_fixed5(&p); pd_hr_pps_probe_init(&q);
    (void)step(&q,&p,5000,true,true,false);
    assert(step(&q,&p,6000,true,true,false));
    queue_and_transmit(&q,&p,6000);
    assert(!step(&q,&p,6249,true,true,false));
    assert(!step(&q,&p,6250,true,true,false));
    assert(q.state == PD_HR_PPS_PROBE_NO_RESPONSE && q.timeout);
    assert(!q.caps_seen && p.state == PE_READY && p.contract_valid &&
           !p.pps_active);
    assert(!step(&q,&p,40000,true,true,false));
}
static void test_phy_busy_and_cancel(void)
{
    pe_t p; pd_hr_pps_probe_t q;
    recover_fixed5(&p); pd_hr_pps_probe_init(&q);
    (void)step(&q,&p,100,true,true,false);
    assert(step(&q,&p,1100,true,true,false));
    assert(pe_request_source_caps(&p));
    pd_hr_pps_probe_queued(&q,1100);
    assert(!step(&q,&p,1299,true,true,false));
    assert(q.state == PD_HR_PPS_PROBE_TX_QUEUED && !q.sent);
    assert(!step(&q,&p,1300,true,true,false));
    assert(q.state == PD_HR_PPS_PROBE_NO_RESPONSE && q.timeout);
    /* main.c cancels only a still-unsent GET_SOURCE_CAP command. */
    assert(p.command == PE_CMD_GET_SOURCE_CAP);
    p.command = PE_CMD_NONE;
    assert(p.state == PE_READY && p.contract_valid);
}
static void test_already_pps(void)
{
    pe_t p; pd_hr_pps_probe_t q;
    pe_init(&p); pe_event(&p,PE_EV_ATTACH,NULL,0);
    caps_to_ready(&p,&six,10); pd_hr_pps_probe_init(&q);
    assert(!step(&q,&p,100,false,true,false));
    assert(q.state == PD_HR_PPS_PROBE_IDLE);
    assert(!step(&q,&p,200,true,true,false));
    assert(q.state == PD_HR_PPS_PROBE_PPS_ALREADY_READY);
    assert(!step(&q,&p,5000,true,true,false));
    assert(!q.sent);
}
static void test_restart_delay_and_early_pps(void)
{
    pe_t p; pd_hr_pps_probe_t q;
    recover_fixed5(&p); pd_hr_pps_probe_init(&q);
    (void)step(&q,&p,100,true,true,false);
    pe_event(&p,PE_EV_CAPS,&five,200);
    assert(p.state == PE_WAIT_ACCEPT);
    assert(!step(&q,&p,900,true,true,false));
    pe_command_result(&p,true,900);
    pe_event(&p,PE_EV_ACCEPT,NULL,901);
    pe_event(&p,PE_EV_PS_RDY,NULL,910);
    assert(!step(&q,&p,911,true,true,false));
    assert(!step(&q,&p,1910,true,true,false));
    assert(step(&q,&p,1911,true,true,false));
    /* An unsolicited PPS SourceCaps during DELAY makes our probe obsolete. */
    pd_hr_pps_probe_init(&q);
    (void)step(&q,&p,2000,true,true,false);
    pe_event(&p,PE_EV_CAPS,&six,2010);
    (void)step(&q,&p,2011,true,true,false);
    pe_command_result(&p,true,2012);
    pe_event(&p,PE_EV_ACCEPT,NULL,2013);
    pe_event(&p,PE_EV_PS_RDY,NULL,2100);
    assert(!step(&q,&p,2101,true,true,false));
    assert(q.state == PD_HR_PPS_PROBE_PPS_ALREADY_READY && !q.sent);
}
static void test_abort_and_wrap(void)
{
    pe_t p; pd_hr_pps_probe_t q;
    recover_fixed5(&p); pd_hr_pps_probe_init(&q);
    (void)step(&q,&p,0xfffffff0u,true,true,false);
    assert(!step(&q,&p,0x000003d7u,true,true,false)); /* 999 ms */
    assert(step(&q,&p,0x000003d8u,true,true,false)); /* 1000 ms */
    queue_and_transmit(&q,&p,0x000003d8u);
    pe_event(&p,PE_EV_HARD_RESET_RX,NULL,0x000003d9u);
    assert(!step(&q,&p,0x000003dau,false,true,true));
    assert(q.state == PD_HR_PPS_PROBE_ABORTED);
    assert(!step(&q,&p,0x00010000u,true,true,false));
    recover_fixed5(&p); pd_hr_pps_probe_init(&q);
    (void)step(&q,&p,0,true,true,false);
    assert(!step(&q,&p,100,false,false,false));
    assert(q.state == PD_HR_PPS_PROBE_ABORTED);
}
static void test_no_unsolicited_probe_before_hr(void)
{
    pe_t p; pd_hr_pps_probe_t q;
    pe_init(&p);
    pe_event(&p,PE_EV_ATTACH,NULL,0);
    caps_to_ready(&p,&five,100);
    pd_hr_pps_probe_init(&q);
    assert(!step(&q,&p,20000,false,true,false));
    assert(q.state == PD_HR_PPS_PROBE_IDLE);
}
int main(void)
{
    test_pps_appears();
    test_fixed_only();
    test_no_response();
    test_phy_busy_and_cancel();
    test_already_pps();
    test_restart_delay_and_early_pps();
    test_abort_and_wrap();
    test_no_unsolicited_probe_before_hr();
    puts("PASS: M13a-7 once-only post-Hard-Reset PPS reprobe (8 scenario groups)");
    return 0;
}
