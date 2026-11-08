/* M13c: real PE + PHY mock. ADC failure and ordinary PE timeouts must
   prevent all subsequent PD transmissions/GoodCRC until CC detach. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ch32fun.h"
#include "pd_policy.h"

mock_systick_t mock_systick;
mock_rcc_t mock_rcc;
mock_gpio_t mock_gpioc;
mock_afio_t mock_afio;
mock_usbpd_t mock_usbpd;
static bool irq_enabled;
void NVIC_DisableIRQ(int irq) { assert(irq == USBPD_IRQn); irq_enabled = false; }
void NVIC_EnableIRQ(int irq) { assert(irq == USBPD_IRQn); irq_enabled = true; }
void Delay_Us(uint32_t us) { (void)us; }
#include "../src/pd_phy_ch32x035.c"

static const pe_caps_t caps4 = { .pdo = {
    0x0a01912cu,0x0002d0deu,0x0003c0a7u,0xc0dc3224u
}, .count = 4u };
static void setup(void) {
    memset(&mock_systick,0,sizeof(mock_systick));
    memset(&mock_rcc,0,sizeof(mock_rcc));
    memset(&mock_gpioc,0,sizeof(mock_gpioc));
    memset(&mock_afio,0,sizeof(mock_afio));
    memset(&mock_usbpd,0,sizeof(mock_usbpd));
    phy_init();
    USBPD->PORT_CC1 = PA_CC_AI;
    phy_poll_cc(1u); phy_poll_cc(25u);
    assert(phy_attached() && irq_enabled);
    pe_event_t ev; pe_caps_t caps;
    assert(phy_pop(&ev,&caps) && ev == PE_EV_ATTACH);
}
static void ready(pe_t *p) {
    pe_init(p);
    pe_event(p,PE_EV_ATTACH,NULL,25u);
    pe_event(p,PE_EV_CAPS,&caps4,30u);
    assert(p->command == PE_CMD_REQUEST);
    pe_command_result(p,true,31u);
    pe_event(p,PE_EV_ACCEPT,NULL,32u);
    pe_event(p,PE_EV_PS_RDY,NULL,34u);
    assert(p->state == PE_READY && p->contract_valid && p->pps_active);
}
static void assert_stopped(void) {
    assert(phy_vbus_loss_latched() && !phy_hard_reset_seen());
    assert(!phy_send(PE_CMD_REQUEST,0x4302bc14u,60u));
    uint16_t h = hdr(PD_CTRL_ACCEPT,0u,4u);
    memcpy(dma_buffer,&h,2u);
    USBPD->BMC_BYTE_CNT=6u;
    USBPD->STATUS=IF_RX_ACT | BMC_AUX_SOP0;
    USBPD_IRQHandler(); USBPD->STATUS=0;
    pe_event_t ev; pe_caps_t caps;
    assert(!phy_pop(&ev,&caps));
    assert(!tx_wait_crc && !crc_wait_armed && retry_count==0u);
}
static void detached_and_rearmed(void) {
    USBPD->PORT_CC1 = 0u;
    phy_poll_cc(100u); phy_poll_cc(260u);
    assert(!phy_attached());
    pe_event_t ev; pe_caps_t caps;
    assert(phy_pop(&ev,&caps) && ev == PE_EV_DETACH);
    phy_reset_after_detach();
    assert(!phy_vbus_loss_latched());
    USBPD->PORT_CC1 = PA_CC_AI;
    phy_poll_cc(280u); phy_poll_cc(305u);
    assert(phy_pop(&ev,&caps) && ev == PE_EV_ATTACH);
    assert(phy_attached() && irq_enabled);
}
int main(void) {
    pe_t p;
    setup(); ready(&p);
    assert(phy_send(PE_CMD_REQUEST,p.rdo,40u));
    assert(tx_wait_crc);
    assert(phy_abort_for_adc_failure());
    pe_event(&p,PE_EV_SENSOR_FAULT,NULL,42u);
    assert(p.state==PE_RECOVERY && !p.contract_valid && !p.pps_active);
    assert(p.command==PE_CMD_NONE && p.faults==1u);
    assert(!phy_abort_for_adc_failure()); /* stop once */
    assert_stopped();
    detached_and_rearmed();

    setup(); ready(&p);
    /* Ordinary PPS Request timeout, unrelated to a Hard Reset. */
    pe_tick(&p,5035u);
    assert(p.state==PE_WAIT_ACCEPT && p.command==PE_CMD_REQUEST);
    pe_command_result(&p,true,5036u);
    pe_tick(&p,5082u);
    assert(p.state==PE_RECOVERY && !p.contract_valid);
    assert(phy_abort_for_policy_failure(PE_WAIT_ACCEPT));
    assert(!phy_abort_for_policy_failure(PE_WAIT_ACCEPT));
    assert_stopped();
    detached_and_rearmed();
    puts("M13c ADC failure and generic PE timeout fail-closed, detach rearm: PASS");
    return 0;
}
