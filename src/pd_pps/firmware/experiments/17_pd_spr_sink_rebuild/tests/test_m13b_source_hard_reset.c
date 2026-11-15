/* M13b host integration: real PHY IRQ + policy + VBUS observer + coordinator. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ch32fun.h"
#include "pd_hr_recovery.h"

mock_systick_t mock_systick;
mock_rcc_t mock_rcc;
mock_gpio_t mock_gpioc;
mock_afio_t mock_afio;
mock_usbpd_t mock_usbpd;
static bool irq_enabled;
void NVIC_DisableIRQ(int irq) { assert(irq==USBPD_IRQn); irq_enabled=false; }
void NVIC_EnableIRQ(int irq) { assert(irq==USBPD_IRQn); irq_enabled=true; }
void Delay_Us(uint32_t delay) { (void)delay; }
#include "../src/pd_phy_ch32x035.c"

static void fixture(void) {
    memset(&mock_systick,0,sizeof(mock_systick));
    memset(&mock_rcc,0,sizeof(mock_rcc));
    memset(&mock_gpioc,0,sizeof(mock_gpioc));
    memset(&mock_afio,0,sizeof(mock_afio));
    memset(&mock_usbpd,0,sizeof(mock_usbpd));
    phy_init(); trace_head=trace_tail=0; trace_drops=0;
    USBPD->PORT_CC1=PA_CC_AI;
    phy_poll_cc(1); phy_poll_cc(25);
    assert(phy_attached() && irq_enabled);
    pe_event_t ev; pe_caps_t caps;
    assert(phy_pop(&ev,&caps) && ev==PE_EV_ATTACH);
}
static void irq(uint16_t status) {
    USBPD->STATUS=status;
    USBPD_IRQHandler();
    USBPD->STATUS=0;
}
static void tx_end(void) { irq(IF_TX_END); }
static void rx_control(uint8_t type, uint8_t mid) {
    uint16_t h=(uint16_t)(0x00a0u | type | ((uint16_t)mid << 9));
    memset(dma_buffer,0,sizeof(dma_buffer));
    memcpy(dma_buffer,&h,2);
    USBPD->BMC_BYTE_CNT=6;
    irq(IF_RX_ACT);
}
static const uint32_t pps_caps[]={
    0x0a01912cu,0x0002d0deu,0x0003c0a7u,0xc0dc3224u
};
static const uint32_t fixed_caps[]={
    0x0801912cu,0x0002d12cu,0x0003c12cu,0x0004b12cu,0x0006412cu
};
static void source_caps(uint8_t mid, bool pps) {
    uint8_t count=pps?4u:5u;
    uint16_t h=(uint16_t)(0x00a1u | ((uint16_t)count<<12) |
                           ((uint16_t)mid<<9));
    memset(dma_buffer,0,sizeof(dma_buffer));
    memcpy(dma_buffer,&h,2);
    for (uint8_t i=0;i<count;i++) {
        uint32_t p=pps?pps_caps[i]:fixed_caps[i];
        memcpy(&dma_buffer[2u+i*4u],&p,4);
    }
    USBPD->BMC_BYTE_CNT=(uint8_t)(6u+count*4u);
    irq(IF_RX_ACT);
}
static void settle(pe_t *p, uint32_t now, bool expect_pps) {
    pe_event_t ev; pe_caps_t caps;
    assert(phy_pop(&ev,&caps) && ev==PE_EV_CAPS);
    pe_event(p,ev,&caps,now);
    assert(p->state==PE_WAIT_ACCEPT && p->command==PE_CMD_REQUEST);
    assert(p->pending_pps==expect_pps);
    assert(p->rdo == (expect_pps?0x4302bc14u:0x13019064u));
    assert(phy_send(p->command,p->rdo,now+1u));
    pe_command_result(p,true,now+1u);
    tx_end();
    rx_control(PD_CTRL_GOODCRC,0u);
    assert(!tx_wait_crc && tx_msg_id==1u);
    rx_control(PD_CTRL_ACCEPT,1u);
    assert(phy_pop(&ev,&caps) && ev==PE_EV_ACCEPT);
    pe_event(p,ev,NULL,now+10u);
    assert(p->state==PE_WAIT_PS_RDY);
    tx_end();
    rx_control(PD_CTRL_PS_RDY,2u);
    assert(phy_pop(&ev,&caps) && ev==PE_EV_PS_RDY);
    pe_event(p,ev,NULL,now+100u);
    assert(p->state==PE_READY && p->contract_valid && p->pps_active==expect_pps);
    assert(p->requested_mv==(expect_pps?7000u:5000u));
}
static void source_hr(bool pps) {
    fixture();
    pe_t p; pe_init(&p); pe_event(&p,PE_EV_ATTACH,NULL,1);
    p.state=PE_READY; p.contract_valid=true; p.pps_active=true;
    pd_hr_recovery_t r; pd_hr_recovery_init(&r);
    pd_hr_recovery_new_session(&r);
    /* Drop a queued pre-reset caps notification: it belongs to the old epoch. */
    source_caps(5u,true); tx_end();
    irq(IF_RX_RESET);
    assert(phy_hard_reset_seen());
    assert(!phy_send(PE_CMD_REQUEST,0x4302bc14u,100));
    pe_event_t ev; pe_caps_t caps;
    assert(phy_pop(&ev,&caps) && ev==PE_EV_HARD_RESET_RX);
    assert(!phy_pop(&ev,&caps));
    pe_event(&p,ev,NULL,100);
    assert(p.state==PE_RECOVERY && !p.contract_valid && p.awaiting_hr_recovery);
    assert(pd_hr_recovery_begin(&r));
    pd_hr_vbus_t h; pd_hr_vbus_init(&h); pd_hr_vbus_begin(&h,100);
    pd_hr_vbus_sample(&h,110,791);
    pd_hr_vbus_sample(&h,120,12);
    pd_hr_vbus_sample(&h,130,12);
    pd_hr_vbus_sample(&h,140,12);
    assert(pd_hr_recovery_step(&r,&h,true,true,false)==PD_HR_REC_ARM_RX);
    assert(phy_hr_arm_rx()); pd_hr_recovery_armed(&r);
    source_caps(0,pps); tx_end();
    assert(!phy_pop(&ev,&caps)); /* staged, not negotiated while VBUS low */
    pd_hr_vbus_sample(&h,1000,570);
    pd_hr_vbus_sample(&h,1010,571);
    pd_hr_vbus_sample(&h,1020,570);
    assert(pd_hr_recovery_step(&r,&h,true,true,false)==PD_HR_REC_COMMIT);
    assert(phy_hr_complete_recovery());
    pe_event(&p,PE_EV_HR_VBUS_READY,NULL,1020);
    pd_hr_recovery_completed(&r);
    assert(p.state==PE_WAIT_CAPS);
    settle(&p,1021,pps);
    assert(!phy_hard_reset_seen());
    assert(!pd_hr_recovery_begin(&r) && r.failed); /* same CC session */
    /* Source HR again: never start a second recovery while CC is held. */
    irq(IF_RX_RESET);
    assert(phy_pop(&ev,&caps) && ev==PE_EV_HARD_RESET_RX);
    pe_event(&p,ev,NULL,1200);
    assert(!pd_hr_recovery_begin(&r));
    phy_hr_fail_stop();
    assert(!phy_hr_complete_recovery() && phy_hard_reset_seen());
    assert(!phy_send(PE_CMD_REQUEST,0x4302bc14u,1201));
    /* A genuine CC detach permits a new session; a Hard Reset does not. */
    USBPD->PORT_CC1=0;
    phy_poll_cc(2000); phy_poll_cc(2160);
    assert(phy_pop(&ev,&caps) && ev==PE_EV_DETACH);
    phy_reset_after_detach();
    USBPD->PORT_CC1=PA_CC_AI;
    phy_poll_cc(2500); phy_poll_cc(2525);
    assert(phy_pop(&ev,&caps) && ev==PE_EV_ATTACH);
    pd_hr_recovery_new_session(&r);
    assert(pd_hr_recovery_begin(&r));
    puts(pps?"PASS: Source HR -> 0V/5V -> PPS 7V Ready; 2nd HR stop; CC rearm":
             "PASS: Source HR -> 0V/5V -> fixed 5V Ready; 2nd HR stop; CC rearm");
}
int main(void) {
    source_hr(true);
    source_hr(false);
    puts("M13b Source-originated HR integration: PASS");
    return 0;
}
