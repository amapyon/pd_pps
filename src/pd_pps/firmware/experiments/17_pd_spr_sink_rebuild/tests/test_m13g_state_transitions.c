/* M13g: exercise actual production Policy Engine, no fault-injection hooks. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "pd_policy.h"
#define CHECK(expr) do { if (!(expr)) { \
 fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr); return false; \
} } while (0)
static const pe_caps_t caps = {.pdo={0x0801912cu,0x0002d12cu,0x0003c12cu,
    0x0004b12cu,0x000641f4u,0xc1a4213cu},.count=6u};
static const pe_caps_t fixed = {.pdo={0x0801912cu},.count=1u};
static void ready_pps(pe_t *p,uint32_t now) {
    pe_init(p); pe_event(p,PE_EV_ATTACH,NULL,now-30u);
    pe_event(p,PE_EV_CAPS,&caps,now-20u);
    pe_command_result(p,true,now-19u);
    pe_event(p,PE_EV_ACCEPT,NULL,now-10u);
    pe_event(p,PE_EV_PS_RDY,NULL,now);
}
static bool ready(const pe_t *p) {return p->state==PE_READY && p->contract_valid &&
    p->pps_active && p->command==PE_CMD_NONE;}
static bool stopped(const pe_t *p) {return p->state==PE_RECOVERY && !p->contract_valid &&
    !p->pps_active && p->command==PE_CMD_NONE;}
static bool unsent_wait_is_terminal(void) {
    pe_t p;pe_init(&p);pe_event(&p,PE_EV_ATTACH,NULL,0u);
    pe_event(&p,PE_EV_CAPS,&caps,10u);
    CHECK(p.command==PE_CMD_REQUEST && !p.request_sent);
    pe_event(&p,PE_EV_WAIT,NULL,11u);
    CHECK(stopped(&p) && p.faults==1u);
    pe_tick(&p,100000u);CHECK(p.command==PE_CMD_NONE);return true;
}
static bool unsent_reject_is_terminal(void) {
    pe_t p;pe_init(&p);pe_event(&p,PE_EV_ATTACH,NULL,0u);
    pe_event(&p,PE_EV_CAPS,&caps,10u);
    pe_event(&p,PE_EV_REJECT,NULL,11u);
    CHECK(stopped(&p) && p.faults==1u);return true;
}
static bool refresh_reject_is_terminal(void) {
    pe_t p;ready_pps(&p,100u);CHECK(ready(&p));
    pe_tick(&p,5100u);CHECK(p.command==PE_CMD_REQUEST);
    pe_command_result(&p,true,5100u);
    pe_event(&p,PE_EV_REJECT,NULL,5101u);
    CHECK(stopped(&p) && p.faults==1u);
    for(uint32_t t=5102u;t<25000u;t+=10u) {pe_tick(&p,t);CHECK(stopped(&p));}
    return true;
}
static bool fixed_contract_renegotiation_reject_terminal(void) {
    pe_t p;pe_init(&p);pe_event(&p,PE_EV_ATTACH,NULL,0u);
    pe_event(&p,PE_EV_CAPS,&fixed,10u);
    pe_command_result(&p,true,11u);
    pe_event(&p,PE_EV_ACCEPT,NULL,12u);
    pe_event(&p,PE_EV_PS_RDY,NULL,13u);
    CHECK(p.state==PE_READY && p.contract_valid && !p.pps_active);
    pe_event(&p,PE_EV_CAPS,&caps,100u);
    pe_command_result(&p,true,101u);
    pe_event(&p,PE_EV_REJECT,NULL,102u);
    CHECK(stopped(&p));return true;
}
static bool repeated_wait_is_bounded(void) {
    pe_t p;ready_pps(&p,100u);
    uint32_t due=5100u;
    for(unsigned i=0;i<3u;i++) {
        pe_tick(&p,due);CHECK(p.state==PE_WAIT_ACCEPT && p.command==PE_CMD_REQUEST);
        pe_command_result(&p,true,due);
        pe_event(&p,PE_EV_WAIT,NULL,due+2u);
        if(i<2u) {
            CHECK(ready(&p) && p.defer_active);
            pe_tick(&p,due+121u);CHECK(ready(&p) && p.command==PE_CMD_NONE);
            due+=122u;
        } else CHECK(stopped(&p) && p.faults==1u);
    }
    pe_tick(&p,due+10000u);CHECK(stopped(&p));return true;
}
static bool successful_renewal_rearms_wait_budget(void) {
    pe_t p;ready_pps(&p,100u);uint32_t due=5100u;
    for(unsigned cycle=0u;cycle<2u;cycle++) {
        for(unsigned w=0u;w<2u;w++) {
            pe_tick(&p,due);CHECK(p.command==PE_CMD_REQUEST);
            pe_command_result(&p,true,due);
            pe_event(&p,PE_EV_WAIT,NULL,due+1u);
            CHECK(ready(&p));due+=121u;
        }
        pe_tick(&p,due);CHECK(p.command==PE_CMD_REQUEST);
        pe_command_result(&p,true,due);
        pe_event(&p,PE_EV_ACCEPT,NULL,due+1u);
        pe_event(&p,PE_EV_PS_RDY,NULL,due+2u);
        CHECK(ready(&p));due+=5002u;
    }
    return true;
}
static bool wait_wrap_and_bound(void) {
    pe_t p;const uint32_t start=0xffffe000u;ready_pps(&p,start);
    uint32_t due=start+5000u;
    for(unsigned i=0u;i<3u;i++) {
        pe_tick(&p,due);CHECK(p.command==PE_CMD_REQUEST);
        pe_command_result(&p,true,due);
        pe_event(&p,PE_EV_WAIT,NULL,due+1u);
        if(i<2u) {CHECK(ready(&p));due+=121u;}
    }
    CHECK(stopped(&p));return true;
}
static bool unsolicited_idle_responses_ignored(void) {
    pe_t p;ready_pps(&p,100u);
    pe_event(&p,PE_EV_REJECT,NULL,110u);
    pe_event(&p,PE_EV_WAIT,NULL,111u);
    pe_event(&p,PE_EV_ACCEPT,NULL,112u);
    pe_event(&p,PE_EV_PS_RDY,NULL,113u);
    CHECK(ready(&p) && p.faults==0u);
    return true;
}
static bool early_accept_not_valid(void) {
    pe_t p;pe_init(&p);pe_event(&p,PE_EV_ATTACH,NULL,0u);
    pe_event(&p,PE_EV_CAPS,&caps,10u);
    pe_event(&p,PE_EV_ACCEPT,NULL,11u);
    CHECK(p.state==PE_WAIT_ACCEPT && !p.request_sent && p.command==PE_CMD_REQUEST);
    pe_command_result(&p,true,12u);
    pe_event(&p,PE_EV_ACCEPT,NULL,13u);
    CHECK(p.state==PE_WAIT_PS_RDY);
    pe_event(&p,PE_EV_PS_RDY,NULL,14u);CHECK(ready(&p));
    return true;
}
static bool late_accept_after_wait_cannot_complete(void) {
    pe_t p;ready_pps(&p,100u);
    pe_tick(&p,5100u);pe_command_result(&p,true,5100u);
    pe_event(&p,PE_EV_WAIT,NULL,5101u);
    CHECK(ready(&p));
    pe_event(&p,PE_EV_ACCEPT,NULL,5102u);
    pe_event(&p,PE_EV_PS_RDY,NULL,5103u);
    CHECK(ready(&p));return true;
}
static bool late_messages_after_stop_ignored(void) {
    pe_t p;ready_pps(&p,100u);
    pe_tick(&p,5100u);pe_command_result(&p,true,5100u);
    pe_event(&p,PE_EV_REJECT,NULL,5101u);CHECK(stopped(&p));
    pe_event(&p,PE_EV_ACCEPT,NULL,5102u);
    pe_event(&p,PE_EV_PS_RDY,NULL,5103u);
    pe_event(&p,PE_EV_CAPS,&caps,5104u);
    pe_event(&p,PE_EV_ATTACH,NULL,5105u);
    CHECK(stopped(&p));return true;
}
static bool physical_reattach_rearms(void) {
    pe_t p;ready_pps(&p,100u);
    pe_tick(&p,5100u);pe_command_result(&p,true,5100u);
    pe_event(&p,PE_EV_REJECT,NULL,5101u);CHECK(stopped(&p));
    pe_event(&p,PE_EV_DETACH,NULL,5102u);
    CHECK(p.state==PE_DETACHED && p.faults==0u);
    pe_event(&p,PE_EV_ATTACH,NULL,5103u);
    pe_event(&p,PE_EV_CAPS,&caps,5104u);
    CHECK(p.command==PE_CMD_REQUEST);
    pe_command_result(&p,true,5105u);
    pe_event(&p,PE_EV_ACCEPT,NULL,5106u);
    pe_event(&p,PE_EV_PS_RDY,NULL,5107u);
    CHECK(ready(&p));return true;
}
static bool first_contract_wait_does_not_form_contract(void) {
    pe_t p;pe_init(&p);pe_event(&p,PE_EV_ATTACH,NULL,0u);
    pe_event(&p,PE_EV_CAPS,&caps,10u);
    pe_command_result(&p,true,11u);pe_event(&p,PE_EV_WAIT,NULL,12u);
    CHECK(p.state==PE_WAIT_CAPS && !p.contract_valid && p.command==PE_CMD_NONE);
    pe_tick(&p,662u);CHECK(stopped(&p));return true;
}
static bool first_contract_reject_does_not_form_contract(void) {
    pe_t p;pe_init(&p);pe_event(&p,PE_EV_ATTACH,NULL,0u);
    pe_event(&p,PE_EV_CAPS,&caps,10u);
    pe_command_result(&p,true,11u);pe_event(&p,PE_EV_REJECT,NULL,12u);
    CHECK(p.state==PE_WAIT_CAPS && !p.contract_valid && p.command==PE_CMD_NONE);
    pe_tick(&p,662u);CHECK(stopped(&p));return true;
}
static bool caps_before_request_can_be_replaced(void) {
    pe_t p;pe_init(&p);pe_event(&p,PE_EV_ATTACH,NULL,0u);
    pe_event(&p,PE_EV_CAPS,&caps,10u);
    pe_event(&p,PE_EV_CAPS,&caps,11u);
    CHECK(p.state==PE_WAIT_ACCEPT && p.command==PE_CMD_REQUEST && !p.request_sent);
    CHECK(p.caps_epoch==2u);return true;
}
static bool caps_after_sent_request_forces_stop(void) {
    pe_t p;pe_init(&p);pe_event(&p,PE_EV_ATTACH,NULL,0u);
    pe_event(&p,PE_EV_CAPS,&caps,10u);pe_command_result(&p,true,11u);
    pe_event(&p,PE_EV_CAPS,&caps,12u);CHECK(stopped(&p));return true;
}
static bool sensor_fault_during_wait_clears_command(void) {
    pe_t p;ready_pps(&p,100u);
    pe_tick(&p,5100u);pe_command_result(&p,true,5100u);
    pe_event(&p,PE_EV_WAIT,NULL,5101u);CHECK(p.defer_active);
    pe_event(&p,PE_EV_SENSOR_FAULT,NULL,5102u);
    CHECK(stopped(&p) && !p.defer_active);
    pe_tick(&p,6000u);CHECK(stopped(&p));return true;
}
static bool soft_reset_preempts_deferred_wait(void) {
    pe_t p;ready_pps(&p,100u);
    pe_tick(&p,5100u);pe_command_result(&p,true,5100u);
    pe_event(&p,PE_EV_WAIT,NULL,5101u);CHECK(p.defer_active);
    pe_event(&p,PE_EV_SOFT_RESET_RX,NULL,5102u);
    CHECK(p.state==PE_WAIT_CAPS && p.command==PE_CMD_ACCEPT_SOFT_RESET);
    CHECK(!p.defer_active && !p.request_sent);
    return true;
}

typedef bool (*testfn)(void);
static const struct {const char *name;testfn fn;} tests[]={
    {"unsent WAIT fails closed",unsent_wait_is_terminal},
    {"unsent REJECT fails closed",unsent_reject_is_terminal},
    {"PPS refresh REJECT stops request storm",refresh_reject_is_terminal},
    {"fixed contract renegotiation REJECT stops",fixed_contract_renegotiation_reject_terminal},
    {"repeated PPS WAIT bounded",repeated_wait_is_bounded},
    {"successful renewal restores WAIT allowance",successful_renewal_rearms_wait_budget},
    {"WAIT bound near 32bit ms wrap",wait_wrap_and_bound},
    {"unsolicited responses in READY ignored",unsolicited_idle_responses_ignored},
    {"early ACCEPT does not complete",early_accept_not_valid},
    {"late ACCEPT after WAIT ignored",late_accept_after_wait_cannot_complete},
    {"late messages after stop ignored",late_messages_after_stop_ignored},
    {"CC reattach restores service",physical_reattach_rearms},
    {"initial WAIT cannot form contract",first_contract_wait_does_not_form_contract},
    {"initial REJECT cannot form contract",first_contract_reject_does_not_form_contract},
    {"CAPS before Request replaces pending",caps_before_request_can_be_replaced},
    {"CAPS after Request forces stop",caps_after_sent_request_forces_stop},
    {"ADC fault during WAIT clears backoff",sensor_fault_during_wait_clears_command},
    {"Soft Reset RX clears WAIT backoff",soft_reset_preempts_deferred_wait},
};
int main(void){unsigned pass=0;
 for(unsigned i=0;i<sizeof tests/sizeof tests[0];i++) {
  if(!tests[i].fn()){fprintf(stderr,"M13g case %u failed: %s\n",i+1u,tests[i].name);return 1;}
  printf("PASS %02u %s\n",i+1u,tests[i].name);pass++;
 }
 printf("M13g state transitions: %u/%zu PASS\n",pass,sizeof tests/sizeof tests[0]);return 0;
}
