/* Use the production CH32X035 USBPD PHY with mocked registers/interrupts. */
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
    pe_event_t ev; pe_caps_t caps;
    assert(phy_attached());
    assert(phy_pop(&ev,&caps) && ev==PE_EV_ATTACH);
}
static void end_tx(void) {
    USBPD->STATUS = IF_TX_END;
    USBPD_IRQHandler();
    USBPD->STATUS=0;
}
static void rx_control(uint8_t type, uint8_t mid) {
    uint16_t header = (uint16_t)(0x00a0u | type | ((uint16_t)mid << 9));
    memset(dma_buffer, 0, sizeof(dma_buffer));
    memcpy(dma_buffer, &header, 2);
    USBPD->BMC_BYTE_CNT=6;
    USBPD->STATUS=IF_RX_ACT;
    USBPD_IRQHandler();
    USBPD->STATUS=0;
}
static uint32_t source_pdos[] = {
    0x0801912cu,0x0002d12cu,0x0003c12cu,0x0004b12cu,0x0006412cu,0xc1a4213cu
};
static void source_caps(uint8_t mid) {
    uint16_t header=(uint16_t)(0x61a1u | ((uint16_t)mid << 9));
    memset(dma_buffer,0,sizeof(dma_buffer));
    memcpy(dma_buffer,&header,2);
    for (int i=0;i<6;i++) memcpy(&dma_buffer[2+4*i],&source_pdos[i],4);
    USBPD->BMC_BYTE_CNT=30;
    USBPD->STATUS=IF_RX_ACT;
    USBPD_IRQHandler();
    USBPD->STATUS=0;
}
static void test_staged(void) {
    fixture();
    assert(phy_send_hard_reset_once());
    end_tx();
    assert(phy_hard_reset_tx_finished());
    assert(!phy_send(PE_CMD_REQUEST,0,0));
    assert(!phy_hr_complete_recovery());
    assert(phy_hr_arm_rx());
    assert(phy_hard_reset_seen() && hr_rx_early);
    source_caps(0);
    assert(hr_early_caps_valid && hr_early_caps.count==6);
    pe_event_t ev; pe_caps_t caps;
    assert(!phy_pop(&ev,&caps));
    assert(!phy_hr_complete_recovery()); /* GoodCRC TX in progress */
    end_tx();
    source_caps(0); /* transport retransmission: GoodCRC but no duplicate event */
    end_tx();
    assert(hr_early_caps_valid);
    assert(phy_hr_complete_recovery());
    assert(!phy_hard_reset_seen() && !hr_rx_early);
    assert(phy_pop(&ev,&caps) && ev==PE_EV_CAPS && caps.count==6);
    assert(caps.msg_id==0 && caps.pdo[5]==source_pdos[5]);
    assert(!phy_pop(&ev,&caps));
    assert(tx_msg_id==0 && last_rx_valid && last_rx_id==0);
    /* A delayed duplicate must still be GoodCRC'd and filtered. */
    source_caps(0);
    end_tx();
    assert(!phy_pop(&ev,&caps));
    /* A fresh MID is a distinct new SourceCaps and is delivered. */
    source_caps(1);
    end_tx();
    assert(phy_pop(&ev,&caps) && ev==PE_EV_CAPS && caps.msg_id==1);
    /* First Request of new contract uses MID=0, as required after HR. */
    assert(phy_send(PE_CMD_REQUEST,0x6302bc14u,1000));
    assert(tx_expected_id==0 && tx_wait_crc);
    puts("PASS PHY: 5V provisional SourceCaps, GoodCRC, hold, duplicate, ID reset");
}
static void test_end_to_end_pps_renegotiation(void) {
    fixture();
    pe_t p; pe_init(&p);
    pe_event(&p, PE_EV_ATTACH, NULL, 1);
    p.state=PE_READY; p.contract_valid=true; p.pps_active=true;
    assert(phy_send_hard_reset_once()); end_tx();
    pe_event(&p, PE_EV_HARD_RESET_TX, NULL, 100);
    assert(p.state==PE_RECOVERY && !p.contract_valid);
    assert(phy_hr_arm_rx());
    source_caps(0); end_tx(); /* staged + GoodCRC */
    assert(phy_hr_complete_recovery());
    pe_event(&p,PE_EV_HR_VBUS_READY,NULL,1000);
    assert(p.state==PE_WAIT_CAPS && !p.contract_valid);
    pe_event_t ev; pe_caps_t caps;
    assert(phy_pop(&ev,&caps) && ev==PE_EV_CAPS);
    pe_event(&p,ev,&caps,1001);
    assert(p.state==PE_WAIT_ACCEPT && p.command==PE_CMD_REQUEST);
    assert(p.rdo==0x6302bc14u);
    assert(phy_send(p.command,p.rdo,1002));
    pe_command_result(&p,true,1002);
    end_tx();
    rx_control(PD_CTRL_GOODCRC,0);
    assert(!tx_wait_crc && tx_msg_id==1);
    rx_control(PD_CTRL_ACCEPT,1);
    assert(phy_pop(&ev,&caps) && ev==PE_EV_ACCEPT);
    pe_event(&p,ev,NULL,1006);
    assert(p.state==PE_WAIT_PS_RDY);
    end_tx();
    rx_control(PD_CTRL_PS_RDY,2);
    assert(phy_pop(&ev,&caps) && ev==PE_EV_PS_RDY);
    pe_event(&p,ev,NULL,1050);
    assert(p.state==PE_READY && p.contract_valid && p.pps_active);
    assert(p.requested_mv==7000 && p.requested_ma==1000);
    puts("PASS E2E: staged caps -> PE WaitCaps -> Request -> GoodCRC -> Accept -> PS_RDY -> Ready");
}
static void test_rx_reset_and_abort(void) {
    fixture();
    assert(phy_send_hard_reset_once()); end_tx();
    assert(phy_hr_arm_rx());
    source_caps(3);
    assert(hr_early_caps_valid);
    USBPD->STATUS=IF_RX_RESET;
    USBPD_IRQHandler();
    USBPD->STATUS=0;
    assert(!hr_rx_early && !hr_early_caps_valid && phy_hard_reset_seen());
    pe_event_t ev; pe_caps_t caps;
    assert(phy_pop(&ev,&caps) && ev==PE_EV_HARD_RESET_RX);
    assert(!phy_pop(&ev,&caps));
    phy_hr_fail_stop();
    assert(!phy_hr_complete_recovery());
    fixture();
    assert(phy_send_hard_reset_once()); end_tx();
    assert(phy_hr_arm_rx());
    source_caps(2); end_tx();
    phy_hr_fail_stop();
    assert(!phy_hr_complete_recovery() && phy_hard_reset_seen());
    assert(!hr_early_caps_valid && !hr_rx_early);
    puts("PASS PHY: second Hard Reset and failure abort leave no stale caps");
}
int main(void){ test_staged();test_end_to_end_pps_renegotiation();test_rx_reset_and_abort();puts("M13a-6 PHY: PASS"); return 0; }
