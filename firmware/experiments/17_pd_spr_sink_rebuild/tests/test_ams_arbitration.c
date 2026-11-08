/* M12d: AMS arbitration tests for the ACTUAL production PE plus PHY.
 * Mocked USBPD registers and injected SOP packets; no real charger/device.
 * Build: sh tests/run_m12d_host_test.sh
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ch32fun.h"
mock_systick_t mock_systick;
mock_rcc_t mock_rcc;
mock_gpio_t mock_gpioc;
mock_afio_t mock_afio;
mock_usbpd_t mock_usbpd;
static bool irq_enabled;
void NVIC_DisableIRQ(int irq) { assert(irq==USBPD_IRQn); irq_enabled=false; }
void NVIC_EnableIRQ(int irq) { assert(irq==USBPD_IRQn); irq_enabled=true; }
void Delay_Us(uint32_t d) { (void)d; }
#include "../src/pd_phy_ch32x035.c"
#define CHECK(c) do { if (!(c)) {fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#c);return false;} } while (0)
static const pe_caps_t caps6 = {.pdo={0x0801912cu,0x0002d12cu,0x0003c12cu,0x0004b12cu,0x0006412cu,0xc1a4213cu},.count=6u,.msg_id=0u};
static void pe_ready(pe_t *p) {
    pe_init(p);
    pe_event(p, PE_EV_ATTACH, NULL, 0u);
    pe_event(p, PE_EV_CAPS, &caps6, 10u);
    assert(p->state == PE_WAIT_ACCEPT && p->command == PE_CMD_REQUEST);
    pe_command_result(p, true, 10u);
    pe_event(p, PE_EV_ACCEPT, NULL, 11u);
    pe_event(p, PE_EV_PS_RDY, NULL, 30u);
    assert(p->state == PE_READY && p->contract_valid && p->pps_active);
}
static void reset_phy(void) {
    memset(&mock_systick,0,sizeof mock_systick);
    memset(&mock_rcc,0,sizeof mock_rcc);
    memset(&mock_gpioc,0,sizeof mock_gpioc);
    memset(&mock_afio,0,sizeof mock_afio);
    memset(&mock_usbpd,0,sizeof mock_usbpd);
    irq_enabled=false; phy_init(); trace_head=trace_tail=0;trace_drops=0;
    USBPD->PORT_CC1 |= PA_CC_AI;
    phy_poll_cc(1u);phy_poll_cc(21u);
    assert(phy_attached() && irq_enabled);
    pe_event_t e;pe_caps_t c;
    assert(phy_pop(&e,&c) && e==PE_EV_ATTACH);
}
static void tx_done(void) {
    USBPD->STATUS=IF_TX_END; USBPD_IRQHandler(); USBPD->STATUS=0;
}
static uint16_t src_header(uint8_t type,uint8_t ndo,uint8_t mid) {
    return (uint16_t)(type|(1u<<5)|(2u<<6)|(1u<<8)|((uint16_t)(mid&7u)<<9)|((uint16_t)ndo<<12));
}
static void rx_src(uint8_t type,uint8_t ndo,uint8_t mid,const pe_caps_t *caps) {
    uint16_t h=src_header(type,ndo,mid);
    memcpy(dma_buffer,&h,2u);
    if(caps)for(uint8_t i=0;i<ndo;i++)memcpy(&dma_buffer[2u+4u*i],&caps->pdo[i],4u);
    USBPD->BMC_BYTE_CNT=(uint32_t)(6u+4u*ndo);
    USBPD->STATUS=IF_RX_ACT|BMC_AUX_SOP0;
    USBPD_IRQHandler();USBPD->STATUS=0u;
}
static bool test_ready_new_caps_same_values_are_re_evaluated(void) {
    pe_t p;pe_ready(&p);
    pe_caps_t c=caps6;c.msg_id=3u;
    pe_event(&p,PE_EV_CAPS,&c,100u);
    CHECK(p.state==PE_WAIT_ACCEPT && p.command==PE_CMD_REQUEST);
    CHECK(p.rdo==0x6302bc14u && p.caps.msg_id==3u && p.caps_epoch==2u);
    pe_command_result(&p,true,100u);
    pe_event(&p,PE_EV_ACCEPT,NULL,101u);
    pe_event(&p,PE_EV_PS_RDY,NULL,120u);
    CHECK(p.state==PE_READY && p.contract_valid);
    return true;
}
static bool test_pending_unsent_request_is_superseded(void) {
    pe_t p;pe_init(&p);pe_event(&p,PE_EV_ATTACH,NULL,0u);
    pe_event(&p,PE_EV_CAPS,&caps6,1u);
    CHECK(!p.request_sent && p.command==PE_CMD_REQUEST);
    pe_caps_t c=caps6;c.msg_id=4u;
    pe_event(&p,PE_EV_CAPS,&c,2u);
    CHECK(p.state==PE_WAIT_ACCEPT && p.command==PE_CMD_REQUEST);
    CHECK(!p.request_sent && p.caps.msg_id==4u);
    CHECK(p.caps_epoch==2u && p.faults==0u);
    return true;
}
static bool test_sent_request_cannot_be_overwritten_by_caps(void) {
    pe_t p;pe_ready(&p);
    pe_caps_t c=caps6;c.msg_id=3u;
    pe_event(&p,PE_EV_CAPS,&c,100u);
    pe_command_result(&p,true,100u);
    CHECK(p.state==PE_WAIT_ACCEPT && p.request_sent && p.command==PE_CMD_NONE);
    c.msg_id=4u;
    pe_event(&p,PE_EV_CAPS,&c,101u);
    CHECK(p.state==PE_RECOVERY && p.command==PE_CMD_NONE);
    CHECK(!p.contract_valid && !p.pps_active && p.faults==1u);
    return true;
}
static bool test_power_transition_rejects_overlapping_caps(void) {
    pe_t p;pe_ready(&p);
    pe_caps_t c=caps6;c.msg_id=2u;
    pe_event(&p,PE_EV_CAPS,&c,100u);
    pe_command_result(&p,true,100u);
    pe_event(&p,PE_EV_ACCEPT,NULL,101u);
    CHECK(p.state==PE_WAIT_PS_RDY && p.request_sent);
    c.msg_id=3u;
    pe_event(&p,PE_EV_CAPS,&c,102u);
    CHECK(p.state==PE_RECOVERY && p.command==PE_CMD_NONE);
    CHECK(!p.contract_valid && !p.pps_active && p.faults==1u);
    pe_event(&p,PE_EV_PS_RDY,NULL,103u);
    CHECK(p.state==PE_RECOVERY && !p.contract_valid);
    return true;
}
static bool test_source_caps_cannot_override_pending_softreset_accept(void) {
    pe_t p;pe_ready(&p);
    pe_event(&p,PE_EV_SOFT_RESET_RX,NULL,100u);
    CHECK(p.state==PE_WAIT_CAPS && p.command==PE_CMD_ACCEPT_SOFT_RESET);
    pe_caps_t c=caps6;c.msg_id=0u;
    pe_event(&p,PE_EV_CAPS,&c,101u);
    CHECK(p.state==PE_RECOVERY && p.command==PE_CMD_NONE);
    CHECK(!p.contract_valid && p.faults==1u);
    return true;
}
static bool test_local_softreset_wait_does_not_start_caps_ams(void) {
    pe_t p;pe_ready(&p);
    CHECK(pe_request_soft_reset(&p,100u));
    pe_command_result(&p,true,100u);
    CHECK(p.state==PE_SOFT_RESET_WAIT);
    pe_caps_t c=caps6;c.msg_id=0u;
    pe_event(&p,PE_EV_CAPS,&c,101u);
    CHECK(p.state==PE_RECOVERY && p.command==PE_CMD_NONE);
    CHECK(!p.contract_valid && p.faults==1u);
    return true;
}
static bool test_softreset_rx_preempts_pending_request_without_tx(void) {
    reset_phy();pe_t p;pe_ready(&p);
    pe_caps_t c=caps6;c.msg_id=3u;
    pe_event(&p,PE_EV_CAPS,&c,100u);
    CHECK(p.command==PE_CMD_REQUEST && !p.request_sent);
    rx_src(PD_CTRL_SOFT_RESET,0u,4u,NULL);
    CHECK(irq_enabled);
    tx_done(); /* GoodCRC to inbound Soft Reset completed. */
    pe_event_t ev;pe_caps_t unused;
    CHECK(phy_pop(&ev,&unused) && ev==PE_EV_SOFT_RESET_RX);
    pe_event(&p,ev,NULL,101u);
    CHECK(p.state==PE_WAIT_CAPS && p.command==PE_CMD_ACCEPT_SOFT_RESET);
    CHECK(!p.request_sent && tx_msg_id==0u);
    CHECK(phy_send(p.command,p.rdo,101u));
    pe_command_result(&p,true,101u);
    uint16_t h;memcpy(&h,dma_buffer,2u);
    CHECK((h&0x1fu)==PD_CTRL_ACCEPT && ((h>>12)&7u)==0u && ((h>>9)&7u)==0u);
    CHECK(p.state==PE_WAIT_CAPS && p.command==PE_CMD_NONE);
    return true;
}
static bool test_softreset_rx_preempts_wait_psrdy(void) {
    pe_t p;pe_ready(&p);
    pe_caps_t c=caps6;c.msg_id=3u;
    pe_event(&p,PE_EV_CAPS,&c,100u);
    pe_command_result(&p,true,100u);
    pe_event(&p,PE_EV_ACCEPT,NULL,101u);
    CHECK(p.state==PE_WAIT_PS_RDY);
    pe_event(&p,PE_EV_SOFT_RESET_RX,NULL,102u);
    CHECK(p.state==PE_WAIT_CAPS && p.command==PE_CMD_ACCEPT_SOFT_RESET);
    CHECK(p.faults==0u);
    return true;
}
static bool test_hardreset_and_detach_cancel_commands(void) {
    pe_t p;pe_ready(&p);
    CHECK(pe_request_source_caps(&p));
    CHECK(p.command==PE_CMD_GET_SOURCE_CAP);
    pe_event(&p,PE_EV_HARD_RESET_RX,NULL,100u);
    CHECK(p.state==PE_RECOVERY && p.command==PE_CMD_NONE && !p.contract_valid);
    pe_event(&p,PE_EV_CAPS,&caps6,101u);
    CHECK(p.state==PE_RECOVERY && p.command==PE_CMD_NONE);
    pe_event(&p,PE_EV_DETACH,NULL,102u);
    CHECK(p.state==PE_DETACHED && p.command==PE_CMD_NONE && p.faults==0u);
    return true;
}
static bool test_recovery_does_not_emit_another_request(void) {
    pe_t p;pe_ready(&p);
    pe_caps_t c=caps6;c.msg_id=5u;
    pe_event(&p,PE_EV_CAPS,&c,100u);
    pe_command_result(&p,true,100u);
    pe_event(&p,PE_EV_ACCEPT,NULL,101u);
    pe_event(&p,PE_EV_CAPS,&c,102u);
    CHECK(p.state==PE_RECOVERY);
    pe_tick(&p,9000u);
    CHECK(p.command==PE_CMD_NONE && !p.contract_valid && !p.pps_active);
    return true;
}

static bool test_new_sop_discards_stale_pending_request(void) {
    reset_phy();
    CHECK(phy_send(PE_CMD_REQUEST,0x6302bc14u,10u));
    tx_done(); /* Outgoing Request physically finished; GoodCRC still missing. */
    CHECK(tx_wait_crc && crc_wait_armed && tx_msg_id==0u);
    rx_src(PD_DATA_SOURCE_CAP,6u,3u,&caps6);
    CHECK(!tx_wait_crc && !crc_wait_armed && retry_count==0u);
    CHECK(tx_msg_id==1u);
    tx_done(); /* Inbound SourceCaps GoodCRC TX ends. */
    SysTick->CNTL += CRC_RECEIVE_TICKS * 5u;
    phy_maintain(5000u);
    CHECK(retry_count==0u && !tx_wait_crc);
    pe_event_t ev;pe_caps_t c;
    CHECK(phy_pop(&ev,&c) && ev==PE_EV_CAPS && c.msg_id==3u);
    CHECK(!phy_pop(&ev,&c));
    return true;
}
static bool test_duplicate_sop_does_not_discard_pending_request(void) {
    reset_phy();
    rx_src(PD_DATA_SOURCE_CAP,6u,3u,&caps6);
    tx_done(); /* GoodCRC for accepted SourceCaps. */
    pe_event_t ev;pe_caps_t c;
    CHECK(phy_pop(&ev,&c) && ev==PE_EV_CAPS);
    CHECK(phy_send(PE_CMD_REQUEST,0x6302bc14u,10u));
    tx_done();
    CHECK(tx_wait_crc && crc_wait_armed && tx_msg_id==0u);
    rx_src(PD_DATA_SOURCE_CAP,6u,3u,&caps6); /* transport duplicate */
    CHECK(tx_wait_crc && tx_msg_id==0u);
    CHECK(!phy_pop(&ev,&c));
    return true;
}
static bool test_softreset_cancels_pending_tx_and_resets_mid(void) {
    reset_phy();
    tx_msg_id=5u;
    CHECK(phy_send(PE_CMD_REQUEST,0x6302bc14u,10u));
    tx_done();
    CHECK(tx_wait_crc && crc_wait_armed && tx_msg_id==5u);
    rx_src(PD_CTRL_SOFT_RESET,0u,2u,NULL);
    CHECK(!tx_wait_crc && !crc_wait_armed && tx_msg_id==0u);
    CHECK(retry_count==0u && !last_rx_valid);
    tx_done();
    SysTick->CNTL += CRC_RECEIVE_TICKS * 3u;
    phy_maintain(10000u);
    CHECK(!tx_wait_crc && retry_count==0u);
    pe_event_t ev;pe_caps_t c;
    CHECK(phy_pop(&ev,&c) && ev==PE_EV_SOFT_RESET_RX);
    return true;
}

int main(void) {
    struct {const char *name; bool (*fn)(void);} tests[]={
        {"READY new caps starts next negotiation",test_ready_new_caps_same_values_are_re_evaluated},
        {"unsent Request can be replaced",test_pending_unsent_request_is_superseded},
        {"sent Request cannot be overwritten",test_sent_request_cannot_be_overwritten_by_caps},
        {"transitioning supply blocks new caps",test_power_transition_rejects_overlapping_caps},
        {"soft reset Accept pending blocks caps",test_source_caps_cannot_override_pending_softreset_accept},
        {"local SoftReset awaiting Accept blocks caps",test_local_softreset_wait_does_not_start_caps_ams},
        {"received SoftReset cancels pending Request",test_softreset_rx_preempts_pending_request_without_tx},
        {"received SoftReset interrupts PS_RDY wait",test_softreset_rx_preempts_wait_psrdy},
        {"HardReset and detach clear pending",test_hardreset_and_detach_cancel_commands},
        {"RECOVERY never schedules fresh Request",test_recovery_does_not_emit_another_request},
        {"new SOP discards pending Request",test_new_sop_discards_stale_pending_request},
        {"duplicate SOP does not discard",test_duplicate_sop_does_not_discard_pending_request},
        {"SoftReset RX cancels pending TX and MID",test_softreset_cancels_pending_tx_and_resets_mid},
    };
    unsigned pass=0u;
    for(unsigned i=0u;i<sizeof(tests)/sizeof(tests[0]);i++) {
        if (tests[i].fn()) {printf("PASS %s\n",tests[i].name);pass++;}
        else printf("FAIL %s\n",tests[i].name);
    }
    printf("M12d AMS arbitration: %u/%zu PASS\n",pass,sizeof(tests)/sizeof(tests[0]));
    return pass==sizeof(tests)/sizeof(tests[0])?0:1;
}
