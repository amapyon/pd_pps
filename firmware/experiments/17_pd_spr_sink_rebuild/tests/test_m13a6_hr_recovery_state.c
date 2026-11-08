/* Production recovery coordinator + voltage observer + Policy Engine. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "pd_hr_recovery.h"
#include "pd_policy.h"

static void sample(pd_hr_vbus_t *h, uint32_t t, uint16_t raw) {
    pd_hr_vbus_sample(h,t,raw);
}
static void test_happy(void) {
    pe_t p; pe_init(&p);
    pe_event(&p, PE_EV_ATTACH, NULL, 1);
    p.state=PE_READY; p.contract_valid=true; p.pps_active=true;
    pe_event(&p, PE_EV_HARD_RESET_TX, NULL, 100);
    assert(p.state==PE_RECOVERY && p.awaiting_hr_recovery && !p.contract_valid);
    pd_hr_recovery_t ctl; pd_hr_recovery_init(&ctl);
    pd_hr_vbus_t obs; pd_hr_vbus_init(&obs);
    assert(pd_hr_recovery_begin(&ctl));
    pd_hr_vbus_begin(&obs,100);
    assert(pd_hr_recovery_step(&ctl,&obs,true,true,false)==PD_HR_REC_NONE);
    sample(&obs,110,791); sample(&obs,120,17); sample(&obs,130,17);
    assert(obs.state==PD_HR_WAIT_LOW);
    sample(&obs,140,17);
    assert(obs.state==PD_HR_WAIT_5V && obs.low_seen);
    assert(pd_hr_recovery_step(&ctl,&obs,true,true,false)==PD_HR_REC_ARM_RX);
    pd_hr_recovery_armed(&ctl);
    assert(pd_hr_recovery_step(&ctl,&obs,true,true,false)==PD_HR_REC_NONE);
    sample(&obs,960,571);
    assert(pd_hr_recovery_step(&ctl,&obs,true,true,false)==PD_HR_REC_NONE);
    sample(&obs,970,570); sample(&obs,980,572);
    assert(obs.state==PD_HR_5V_OBSERVED && obs.high_seen);
    assert(pd_hr_recovery_step(&ctl,&obs,true,true,false)==PD_HR_REC_COMMIT);
    pe_event(&p, PE_EV_HR_VBUS_READY, NULL, 980);
    assert(p.state==PE_WAIT_CAPS && p.awaiting_hr_recovery==false &&
           !p.contract_valid && p.command==PE_CMD_NONE);
    pd_hr_recovery_completed(&ctl);
    assert(!ctl.active && ctl.completed && ctl.used);
    assert(pd_hr_recovery_step(&ctl,&obs,true,false,false)==PD_HR_REC_NONE);
    pd_hr_recovery_detached(&ctl);
    assert(!ctl.completed && ctl.used);
    /* No second automatic recovery, even after detach or another HR. */
    assert(!pd_hr_recovery_begin(&ctl) && ctl.failed);
    puts("PASS: 0V->5V stable; early RX; post-confirmation PE restart; no rearm");
}
static void test_bad_inputs(void) {
    pd_hr_recovery_t r; pd_hr_vbus_t h;
    pd_hr_recovery_init(&r); pd_hr_vbus_init(&h);
    /* Unexpected event cannot revive a stopped PE. */
    pe_t p; pe_init(&p); pe_event(&p,PE_EV_ATTACH,NULL,0);
    pe_event(&p,PE_EV_VBUS_LOST,NULL,1);
    assert(p.state==PE_RECOVERY);
    pe_event(&p,PE_EV_HR_VBUS_READY,NULL,2);
    assert(p.state==PE_RECOVERY);
    assert(pd_hr_recovery_begin(&r)); pd_hr_vbus_begin(&h,0);
    assert(pd_hr_recovery_step(&r,&h,false,true,false)==PD_HR_REC_STOP);
    assert(pd_hr_recovery_step(&r,&h,true,true,true)==PD_HR_REC_STOP);
    pd_hr_vbus_adc_failed(&h);
    assert(pd_hr_recovery_step(&r,&h,true,true,false)==PD_HR_REC_STOP);
    pd_hr_recovery_failed(&r);
    assert(pd_hr_recovery_step(&r,&h,true,true,false)==PD_HR_REC_NONE);
    pd_hr_recovery_init(&r); pd_hr_vbus_init(&h);
    assert(pd_hr_recovery_begin(&r));
    pd_hr_vbus_begin(&h,0);
    pd_hr_vbus_tick(&h,1000);
    assert(h.state==PD_HR_FAILED && h.error==1);
    assert(pd_hr_recovery_step(&r,&h,true,true,false)==PD_HR_REC_STOP);
    /* 5V before low does not release anything. */
    pd_hr_recovery_init(&r); pd_hr_vbus_init(&h);
    assert(pd_hr_recovery_begin(&r)); pd_hr_vbus_begin(&h,0);
    sample(&h,10,570); sample(&h,20,570); sample(&h,30,570);
    assert(h.state==PD_HR_WAIT_LOW && !h.low_seen);
    assert(pd_hr_recovery_step(&r,&h,true,true,false)==PD_HR_REC_NONE);
    puts("PASS: fail-closed on detach/ADC/timeouts; spurious event; 5V without 0V");
}
static void test_high_timeout_and_streak(void) {
    pd_hr_recovery_t r; pd_hr_vbus_t h;
    pd_hr_recovery_init(&r); pd_hr_vbus_init(&h);
    assert(pd_hr_recovery_begin(&r)); pd_hr_vbus_begin(&h,0xfffffff0u);
    sample(&h,0xfffffff1u,16); sample(&h,0xfffffffb,16); sample(&h,5,16);
    assert(h.low_seen && h.state==PD_HR_WAIT_5V);
    assert(pd_hr_recovery_step(&r,&h,true,true,false)==PD_HR_REC_ARM_RX);
    pd_hr_recovery_armed(&r);
    sample(&h,15,571);
    sample(&h,50,572); /* gap >25 resets streak */
    assert(h.streak==1);
    sample(&h,61,16); /* 0V again resets 5V streak */
    assert(h.streak==0);
    sample(&h,71,571); sample(&h,81,570); sample(&h,91,570);
    assert(h.state==PD_HR_5V_OBSERVED);
    assert(pd_hr_recovery_step(&r,&h,true,true,false)==PD_HR_REC_COMMIT);
    puts("PASS: sample gaps, voltage bounce and 32-bit ms wrap");
}
int main(void) { test_happy(); test_bad_inputs(); test_high_timeout_and_streak(); puts("M13a-6 state: PASS"); return 0; }
