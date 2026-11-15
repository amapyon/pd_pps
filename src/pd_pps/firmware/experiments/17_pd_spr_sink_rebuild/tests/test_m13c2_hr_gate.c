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
void NVIC_DisableIRQ(int irq) { assert(irq == USBPD_IRQn); irq_enabled = false; }
void NVIC_EnableIRQ(int irq) { assert(irq == USBPD_IRQn); irq_enabled = true; }
void Delay_Us(uint32_t delay) { (void)delay; }
#include "../src/pd_phy_ch32x035.c"

static void setup(void) {
    memset(&mock_systick, 0, sizeof(mock_systick));
    memset(&mock_rcc, 0, sizeof(mock_rcc));
    memset(&mock_gpioc, 0, sizeof(mock_gpioc));
    memset(&mock_afio, 0, sizeof(mock_afio));
    memset(&mock_usbpd, 0, sizeof(mock_usbpd));
    phy_init();
    USBPD->PORT_CC1 = PA_CC_AI;
    phy_poll_cc(1u); phy_poll_cc(25u);
    assert(phy_attached() && irq_enabled);
    pe_event_t e; pe_caps_t caps;
    assert(phy_pop(&e,&caps) && e == PE_EV_ATTACH);
    USBPD->STATUS = IF_RX_RESET;
    USBPD_IRQHandler(); USBPD->STATUS = 0u;
    assert(phy_pop(&e,&caps) && e == PE_EV_HARD_RESET_RX);
    assert(phy_hard_reset_seen());
}
static void assert_gated(void) {
    assert(phy_hard_reset_seen());
    assert(!phy_send(PE_CMD_REQUEST, 0x4302bc14u, 90u));
}
static void test_arm_transient_busy_then_success(void) {
    setup();
    pd_hr_recovery_t r; pd_hr_recovery_init(&r);
    assert(pd_hr_recovery_begin(&r));
    USBPD->CONTROL |= PD_TX_EN;
    assert(!phy_hr_arm_rx());
    assert(!pd_hr_recovery_gate_busy(&r, PD_HR_REC_ARM_RX, 100u));
    assert(!pd_hr_recovery_gate_busy(&r, PD_HR_REC_ARM_RX, 119u));
    USBPD->CONTROL &= ~PD_TX_EN;
    assert(phy_hr_arm_rx());
    pd_hr_recovery_gate_ready(&r);
    pd_hr_recovery_armed(&r);
    assert(r.early_rx_armed && !r.gate_waiting);
    assert(phy_hr_complete_recovery());
    pd_hr_recovery_completed(&r);
    assert(!phy_hard_reset_seen() && r.completed && !r.failed);
}
static void test_arm_stuck_busy_stop(void) {
    setup();
    pd_hr_recovery_t r; pd_hr_recovery_init(&r);
    assert(pd_hr_recovery_begin(&r));
    USBPD->CONTROL |= PD_TX_EN;
    assert(!phy_hr_arm_rx());
    assert(!pd_hr_recovery_gate_busy(&r, PD_HR_REC_ARM_RX, UINT32_MAX - 8u));
    assert(!pd_hr_recovery_gate_busy(&r, PD_HR_REC_ARM_RX, 10u));
    assert(pd_hr_recovery_gate_busy(&r, PD_HR_REC_ARM_RX, 11u));
    phy_hr_fail_stop();
    pd_hr_recovery_failed(&r);
    assert(r.failed && !r.active);
    assert_gated();
}
static void test_commit_stuck_stop(void) {
    setup();
    pd_hr_recovery_t r; pd_hr_recovery_init(&r);
    assert(pd_hr_recovery_begin(&r));
    assert(phy_hr_arm_rx()); pd_hr_recovery_armed(&r);
    USBPD->CONTROL |= PD_TX_EN;
    assert(!phy_hr_complete_recovery());
    assert(!pd_hr_recovery_gate_busy(&r, PD_HR_REC_COMMIT, 100u));
    assert(!pd_hr_recovery_gate_busy(&r, PD_HR_REC_COMMIT, 119u));
    assert(pd_hr_recovery_gate_busy(&r, PD_HR_REC_COMMIT, 120u));
    phy_hr_fail_stop(); pd_hr_recovery_failed(&r);
    assert_gated();
}
static void test_quota_and_detach_rearm(void) {
    pd_hr_recovery_t r;
    pd_hr_recovery_init(&r);
    assert(pd_hr_recovery_begin(&r));
    pd_hr_recovery_completed(&r);
    assert(!pd_hr_recovery_begin(&r));
    assert(r.failed);
    pd_hr_recovery_detached(&r);
    assert(!pd_hr_recovery_begin(&r));
    pd_hr_recovery_new_session(&r);
    assert(pd_hr_recovery_begin(&r));
}
int main(void) {
    test_arm_transient_busy_then_success();
    test_arm_stuck_busy_stop();
    test_commit_stuck_stop();
    test_quota_and_detach_rearm();
    puts("M13c-2 real PHY HR gate 20ms timeout, fail-closed, CC quota: PASS");
    return 0;
}
