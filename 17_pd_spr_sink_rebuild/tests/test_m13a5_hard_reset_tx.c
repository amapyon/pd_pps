/* M13a-5: actual production PHY with mocked CH32X035 USBPD registers. */
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
void NVIC_DisableIRQ(int irq) { assert(irq == USBPD_IRQn); irq_enabled=false; }
void NVIC_EnableIRQ(int irq) { assert(irq == USBPD_IRQn); irq_enabled=true; }
void Delay_Us(uint32_t delay) { (void)delay; }
#include "../src/pd_phy_ch32x035.c"

static void fixture(int cc) {
    memset(&mock_systick,0,sizeof(mock_systick));
    memset(&mock_rcc,0,sizeof(mock_rcc));
    memset(&mock_gpioc,0,sizeof(mock_gpioc));
    memset(&mock_afio,0,sizeof(mock_afio));
    memset(&mock_usbpd,0,sizeof(mock_usbpd));
    irq_enabled=false;
    phy_init();
    trace_head=trace_tail=0; trace_drops=0;
    if(cc==1) USBPD->PORT_CC1=PA_CC_AI;
    else USBPD->PORT_CC2=PA_CC_AI;
    phy_poll_cc(1);
    phy_poll_cc(21);
    assert(attached && selected_cc==cc && irq_enabled);
    pe_event_t ev; pe_caps_t caps;
    assert(phy_pop(&ev,&caps) && ev==PE_EV_ATTACH);
    assert(!phy_pop(&ev,&caps));
}
static void mark_tx_end(uint32_t tick) {
    SysTick->CNTL=tick;
    USBPD->STATUS=IF_TX_END;
    USBPD_IRQHandler();
    USBPD->STATUS=0; /* emulate write-one-to-clear */
}
static void test_send_reset_no_sop(void) {
    fixture(1);
    SysTick->CNTL=123456u;
    assert(PE_EV_VBUS_LOST==10 && PE_EV_HARD_RESET_TX==11);
    assert(phy_send_hard_reset_once());
    assert(phy_hard_reset_seen() && hard_reset_tx_active);
    assert(!hard_reset_tx_finished && !hard_reset_tx_timeout);
    assert(USBPD->TX_SEL==0xa9u && USBPD->BMC_TX_SZ==0u);
    assert(USBPD->CONTROL & PD_TX_EN);
    assert(hard_reset_tx_start_tick==123456u);
    assert(!tx_wait_crc && !crc_wait_armed && !last_rx_valid);
    assert(!phy_send(PE_CMD_REQUEST,0x6302bc14,0u));
    assert(!phy_send_hard_reset_once());
    /* PHY does not double-enqueue the TX event. Main handles it once. */
    pe_event_t ev; pe_caps_t caps;
    assert(!phy_pop(&ev,&caps));
    pe_t p; pe_init(&p);
    pe_event(&p,PE_EV_ATTACH,NULL,0);
    p.state=PE_READY; p.contract_valid=true; p.pps_active=true;
    p.rdo=0x6302bc14;
    pe_event(&p,PE_EV_HARD_RESET_TX,NULL,2000);
    assert(p.state==PE_RECOVERY && !p.contract_valid && !p.pps_active);
    assert(p.faults==1);
    mark_tx_end(130000u);
    assert(phy_hard_reset_tx_finished() && !hard_reset_tx_active);
    assert(!(USBPD->CONTROL & PD_TX_EN));
    assert(!crc_wait_armed && !tx_wait_crc);
    assert(!phy_send(PE_CMD_GET_SOURCE_CAP,0,2200));
    SysTick->CNTL+=PD_HARD_RESET_TX_TIMEOUT_TICKS * 2u;
    phy_maintain(5000u);
    assert(!phy_hard_reset_tx_timed_out());
    puts("PASS CC1: 0xa9 reset ordered set; no SOP/GoodCRC; complete; PE fail-closed");
}
static void test_timeout_and_wrap(void) {
    fixture(2);
    SysTick->CNTL=0xffff0000u;
    assert(phy_send_hard_reset_once());
    SysTick->CNTL=0xffff0000u + PD_HARD_RESET_TX_TIMEOUT_TICKS-1u;
    phy_maintain(9999u);
    assert(hard_reset_tx_active && !hard_reset_tx_timeout);
    SysTick->CNTL++;
    phy_maintain(9999u);
    assert(!hard_reset_tx_active && hard_reset_tx_timeout);
    assert(!hard_reset_tx_finished && phy_hard_reset_seen());
    assert(!(USBPD->CONTROL & PD_TX_EN));
    assert(!phy_send_hard_reset_once());
    assert(!phy_send(PE_CMD_REQUEST,0,100));
    puts("PASS CC2: transmission deadline and 32-bit SysTick wrap; no second TX");
}
static void test_must_not_fire_when_busy_or_absent(void) {
    fixture(1);
    assert(phy_send(PE_CMD_REQUEST,0x6302bc14u,0));
    assert(!phy_send_hard_reset_once());
    /* Cancel prior pending request as part of a source-initiated Reset. */
    USBPD->STATUS=IF_RX_RESET;
    USBPD_IRQHandler();
    USBPD->STATUS=0;
    assert(!phy_send_hard_reset_once());
    assert(!hard_reset_tx_active);
    fixture(2);
    assert(phy_abort_for_vbus_loss(17,151));
    assert(!phy_send_hard_reset_once());
    fixture(1);
    phy_poll_cc(30); /* attached CC1; make it absent for 150ms */
    USBPD->PORT_CC1=0;
    phy_poll_cc(31); phy_poll_cc(200);
    assert(!phy_attached());
    assert(!phy_send_hard_reset_once());
    puts("PASS guards: busy, incoming reset, low VBUS and detached");
}
int main(void) {
    test_send_reset_no_sop();
    test_timeout_and_wrap();
    test_must_not_fire_when_busy_or_absent();
    puts("M13a-5 host PHY: PASS");
    return 0;
}
