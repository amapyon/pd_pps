/* Production PE + production PHY with CH32X035 register mocks. */
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
static bool inject_hr_before_irq_mask;
void USBPD_IRQHandler(void);
void NVIC_DisableIRQ(int irq) {
    assert(irq == USBPD_IRQn);
    if (inject_hr_before_irq_mask) {
        inject_hr_before_irq_mask = false;
        USBPD->STATUS = IF_RX_RESET;
        USBPD_IRQHandler();
        USBPD->STATUS = 0u;
    }
    irq_enabled = false;
}
void NVIC_EnableIRQ(int irq) { assert(irq == USBPD_IRQn); irq_enabled = true; }
void Delay_Us(uint32_t n) { (void)n; }
#include "../src/pd_phy_ch32x035.c"

static const pe_caps_t caps6 = {
    .pdo = {0x0801912cu, 0x0002d12cu, 0x0003c12cu,
            0x0004b12cu, 0x0006412cu, 0xc1a4213cu},
    .count = 6u
};
static void setup(void) {
    memset(&mock_systick,0,sizeof(mock_systick));
    memset(&mock_rcc,0,sizeof(mock_rcc));
    memset(&mock_gpioc,0,sizeof(mock_gpioc));
    memset(&mock_afio,0,sizeof(mock_afio));
    memset(&mock_usbpd,0,sizeof(mock_usbpd));
    inject_hr_before_irq_mask = false;
    phy_init();
    USBPD->PORT_CC1 |= PA_CC_AI;
    phy_poll_cc(1u); phy_poll_cc(21u);
    assert(phy_attached() && irq_enabled);
    pe_event_t ev; pe_caps_t caps;
    assert(phy_pop(&ev, &caps) && ev == PE_EV_ATTACH);
}
static void to_ready(pe_t *p) {
    pe_init(p);
    pe_event(p, PE_EV_ATTACH, NULL, 1u);
    pe_event(p, PE_EV_CAPS, &caps6, 2u);
    assert(p->command == PE_CMD_REQUEST);
    pe_command_result(p, true, 3u);
    pe_event(p, PE_EV_ACCEPT, NULL, 4u);
    pe_event(p, PE_EV_PS_RDY, NULL, 5u);
    assert(p->state == PE_READY && p->contract_valid && p->pps_active);
}
static void do_abort(pe_t *p) {
    (void)phy_abort_for_vbus_loss(17u, 151u);
    pe_event(p, PE_EV_VBUS_LOST, NULL, 50u);
}
int main(void) {
    pe_t p; pe_event_t ev; pe_caps_t caps;
    setup(); to_ready(&p);
    assert(phy_send(PE_CMD_REQUEST, p.rdo, 20u));
    USBPD->STATUS = IF_TX_END;
    USBPD_IRQHandler(); USBPD->STATUS = 0u;
    assert(tx_wait_crc && crc_wait_armed);
    enqueue(PE_EV_PS_RDY, NULL); /* stale queued traffic */
    do_abort(&p);
    assert(phy_vbus_loss_latched() && p.state == PE_RECOVERY);
    assert(!p.contract_valid && !p.pps_active && !p.pending_pps);
    assert(p.command == PE_CMD_NONE && p.rdo == 0 && p.caps.count == 0u);
    assert(p.requested_mv == 0u && p.requested_ma == 0u);
    assert(p.faults == 1u && !tx_wait_crc && !crc_wait_armed);
    assert(!(USBPD->CONTROL & PD_TX_EN) && retry_size == 0u);
    assert(!phy_pop(&ev, &caps));
    assert(!phy_send(PE_CMD_REQUEST, 0x6302bc14u, 60u));
    SysTick->CNTL = CRC_RECEIVE_TICKS * 5u;
    phy_maintain(100u);
    assert(!tx_wait_crc && retry_count == 0u);
    /* Incoming packets are ignored, without GoodCRC, while VBUS is absent. */
    uint16_t h = hdr(PD_CTRL_ACCEPT, 0, 6);
    memcpy(dma_buffer, &h, 2);
    USBPD->BMC_BYTE_CNT = 6u;
    USBPD->STATUS = IF_RX_ACT | BMC_AUX_SOP0;
    USBPD_IRQHandler(); USBPD->STATUS = 0;
    assert(!phy_pop(&ev, &caps));
    pe_event(&p, PE_EV_CAPS, &caps6, 150u);
    pe_event(&p, PE_EV_PS_RDY, NULL, 151u);
    pe_tick(&p, 10000u);
    assert(p.state == PE_RECOVERY && p.command == PE_CMD_NONE);
    /* Detecting a second identical fault must not increment counters. */
    do_abort(&p);
    assert(p.faults == 1u);
    /* No voltage-only or timer-only recovery. A real CC detach clears the
       PHY latch and a new attach can start an entirely new negotiation. */
    USBPD->PORT_CC1 &= ~PA_CC_AI;
    phy_poll_cc(200u);
    phy_poll_cc(351u);
    assert(!phy_attached());
    assert(phy_pop(&ev, &caps) && ev == PE_EV_DETACH);
    pe_event(&p, ev, NULL, 351u);
    phy_reset_after_detach();
    assert(p.state == PE_DETACHED && !phy_vbus_loss_latched());
    USBPD->PORT_CC1 |= PA_CC_AI;
    phy_poll_cc(360u); phy_poll_cc(380u);
    assert(phy_pop(&ev, &caps) && ev == PE_EV_ATTACH);
    pe_event(&p, ev, NULL, 380u);
    assert(p.state == PE_WAIT_CAPS);
    /* Hard Reset IRQ immediately before IRQ masking outranks ADC loss. */
    setup(); to_ready(&p);
    inject_hr_before_irq_mask = true;
    assert(!phy_abort_for_vbus_loss(17u, 151u));
    assert(phy_hard_reset_seen() && !phy_vbus_loss_latched());
    assert(phy_pop(&ev, &caps) && ev == PE_EV_HARD_RESET_RX);
    pe_event(&p, ev, NULL, 55u);
    assert(p.state == PE_RECOVERY && !p.contract_valid);
    assert(!phy_send(PE_CMD_REQUEST, 0x6302bc14u, 56u));
    puts("M13a-3 PE/PHY fail-closed VBUS loss: PASS");
    return 0;
}
