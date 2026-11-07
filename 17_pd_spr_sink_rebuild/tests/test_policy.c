#include "pd_policy.h"
#include <assert.h>
#include <stdio.h>
static const pe_caps_t caps = {
    .pdo = {0x0801912c,0x0002d12c,0x0003c12c,0x0004b12c,0x0006412c,0xc1a4213c},
    .count = 6, .msg_id = 0
};
static void initial(pe_t *p) {
    pe_init(p);
    pe_event(p, PE_EV_ATTACH, 0, 0);
    pe_event(p, PE_EV_CAPS, &caps, 5);
    assert(p->state == PE_WAIT_ACCEPT && p->command == PE_CMD_REQUEST);
    assert(p->rdo == 0x6302bc14u);
    pe_command_result(p, true, 5);
    pe_event(p, PE_EV_ACCEPT, 0, 6);
    pe_event(p, PE_EV_PS_RDY, 0, 10);
    assert(p->state == PE_READY && p->pps_active && p->contract_valid);
}
int main(void) {
    pe_t p; initial(&p);
    pe_caps_t second = caps; second.msg_id=1;
    pe_event(&p, PE_EV_CAPS, &second, 20);
    assert(p.state == PE_WAIT_ACCEPT && p.command == PE_CMD_REQUEST);
    pe_command_result(&p, true, 20);
    pe_event(&p, PE_EV_ACCEPT, 0, 22);
    pe_event(&p, PE_EV_PS_RDY, 0, 30);
    assert(p.state == PE_READY);
    pe_tick(&p, 5029);
    assert(p.state == PE_READY);
    pe_tick(&p, 5030);
    assert(p.command == PE_CMD_REQUEST);
    pe_event(&p, PE_EV_HARD_RESET_RX, 0, 5031);
    assert(p.state == PE_RECOVERY && p.command == PE_CMD_NONE && !p.contract_valid);
    pe_event(&p, PE_EV_CAPS, &caps, 5032);
    assert(p.state == PE_RECOVERY);
    pe_event(&p, PE_EV_DETACH, 0, 5033);
    assert(p.state == PE_DETACHED);

    initial(&p);
    assert(pe_request_soft_reset(&p, 50));
    assert(p.command == PE_CMD_SOFT_RESET);
    pe_command_result(&p, true, 50);
    assert(p.state == PE_SOFT_RESET_WAIT);
    pe_event(&p, PE_EV_ACCEPT, 0, 51);
    assert(p.state == PE_WAIT_CAPS);
    pe_event(&p, PE_EV_CAPS, &caps, 52);
    assert(p.command == PE_CMD_REQUEST && p.rdo == 0x6302bc14u);

    initial(&p);
    pe_event(&p, PE_EV_CAPS, &caps, 100);
    pe_command_result(&p, true, 100);
    pe_event(&p, PE_EV_WAIT, 0, 101);
    assert(p.state == PE_READY && p.contract_valid);

    pe_init(&p);
    pe_event(&p, PE_EV_ATTACH, 0, 0);
    pe_caps_t fixed = {.pdo={0x0801912c}, .count=1};
    pe_event(&p, PE_EV_CAPS, &fixed, 10);
    assert(p.command == PE_CMD_REQUEST && !p.pending_pps);
    assert(p.rdo >> 28 == 1);
    printf("PASS: initial PPS, identical capabilities, PPS refresh, resets, Wait, fixed fallback\n");
    return 0;
}
