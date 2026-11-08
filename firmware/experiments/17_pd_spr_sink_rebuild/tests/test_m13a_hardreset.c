/* M13a: exercising the ACTUAL production PE and USBPD PHY with mock registers.
 * No charger or electrical VBUS detection is simulated by this test.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ch32fun.h"

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
    /* Simulate an IRQ arriving immediately before the mask takes effect. */
    if (inject_hr_before_irq_mask) {
        inject_hr_before_irq_mask = false;
        USBPD->STATUS = IF_RX_RESET;
        USBPD_IRQHandler();
        USBPD->STATUS = 0u;
    }
    irq_enabled = false;
}
void NVIC_EnableIRQ(int irq) { assert(irq == USBPD_IRQn); irq_enabled = true; }
void Delay_Us(uint32_t usec) { (void)usec; }
#include "../src/pd_phy_ch32x035.c"

#define CHECK(x) do { if (!(x)) {fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);return false;} } while (0)
static const pe_caps_t caps6 = {
    .pdo = {0x0801912cu, 0x0002d12cu, 0x0003c12cu,
            0x0004b12cu, 0x0006412cu, 0xc1a4213cu},
    .count = 6u, .msg_id = 0u
};
static void pe_ready(pe_t *p) {
    pe_init(p);
    pe_event(p, PE_EV_ATTACH, NULL, 0u);
    pe_event(p, PE_EV_CAPS, &caps6, 1u);
    assert(p->command == PE_CMD_REQUEST);
    pe_command_result(p, true, 1u);
    pe_event(p, PE_EV_ACCEPT, NULL, 2u);
    pe_event(p, PE_EV_PS_RDY, NULL, 4u);
    assert(p->state == PE_READY && p->contract_valid && p->pps_active);
}
static void reset_fixture(void) {
    memset(&mock_systick, 0, sizeof mock_systick);
    memset(&mock_rcc, 0, sizeof mock_rcc);
    memset(&mock_gpioc, 0, sizeof mock_gpioc);
    memset(&mock_afio, 0, sizeof mock_afio);
    memset(&mock_usbpd, 0, sizeof mock_usbpd);
    irq_enabled = false;
    inject_hr_before_irq_mask = false;
    phy_init();
    trace_head = trace_tail = 0; trace_drops = 0;
    USBPD->PORT_CC1 |= PA_CC_AI;
    phy_poll_cc(1u); phy_poll_cc(21u);
    assert(phy_attached() && irq_enabled);
    pe_event_t ev; pe_caps_t c;
    assert(phy_pop(&ev, &c) && ev == PE_EV_ATTACH);
}
static void inject_reset(uint32_t extra_status) {
    USBPD->STATUS = IF_RX_RESET | extra_status;
    USBPD_IRQHandler();
    USBPD->STATUS = 0u; /* Hardware would clear the W1C flags. */
}
static void inject_caps(uint8_t id) {
    const uint16_t h = (uint16_t)(PD_DATA_SOURCE_CAP | (1u << 5) |
                         (2u << 6) | (1u << 8) | ((uint16_t)id << 9) | (6u << 12));
    memcpy(dma_buffer, &h, 2);
    for (uint8_t i = 0; i < 6u; i++) memcpy(&dma_buffer[2u + 4u * i], &caps6.pdo[i], 4);
    USBPD->BMC_BYTE_CNT = 6u + 24u;
    USBPD->STATUS = IF_RX_ACT | BMC_AUX_SOP0;
    USBPD_IRQHandler(); USBPD->STATUS = 0;
}
static bool policy_clears_every_contract_field(void) {
    pe_t p; pe_ready(&p);
    CHECK(p.requested_mv == 7000u && p.requested_ma == 1000u);
    CHECK(pe_request_source_caps(&p));
    pe_event(&p, PE_EV_HARD_RESET_RX, NULL, 10u);
    CHECK(p.state == PE_RECOVERY && !p.contract_valid && !p.pps_active && !p.pending_pps);
    CHECK(p.command == PE_CMD_NONE && !p.request_sent && !p.pending_soft_reset);
    CHECK(p.requested_mv == 0u && p.requested_ma == 0u);
    CHECK(p.rdo == 0u && p.object_pos == 0u && p.caps.count == 0u);
    CHECK(p.caps_epoch == 0u && p.deadline_ms == 0u && p.defer_until_ms == 0u);
    CHECK(p.faults == 1u);
    return true;
}
static bool policy_aborts_pretransmission_request(void) {
    pe_t p; pe_init(&p);
    pe_event(&p, PE_EV_ATTACH, NULL, 0u);
    pe_event(&p, PE_EV_CAPS, &caps6, 1u);
    CHECK(p.command == PE_CMD_REQUEST && !p.request_sent);
    pe_event(&p, PE_EV_HARD_RESET_RX, NULL, 2u);
    CHECK(p.state == PE_RECOVERY && p.command == PE_CMD_NONE);
    CHECK(p.caps.count == 0u && p.rdo == 0u);
    pe_tick(&p, 100000u);
    CHECK(p.state == PE_RECOVERY && p.command == PE_CMD_NONE);
    return true;
}
static bool policy_cannot_accept_stale_psrdy(void) {
    pe_t p; pe_ready(&p);
    pe_event(&p, PE_EV_CAPS, &caps6, 10u);
    pe_command_result(&p, true, 11u);
    pe_event(&p, PE_EV_ACCEPT, NULL, 12u);
    CHECK(p.state == PE_WAIT_PS_RDY);
    pe_event(&p, PE_EV_HARD_RESET_RX, NULL, 13u);
    pe_event(&p, PE_EV_PS_RDY, NULL, 14u);
    pe_event(&p, PE_EV_CAPS, &caps6, 15u);
    CHECK(p.state == PE_RECOVERY && !p.contract_valid && p.command == PE_CMD_NONE);
    return true;
}
static bool reset_discards_stale_rx_queue(void) {
    reset_fixture();
    enqueue(PE_EV_CAPS, &caps6);
    enqueue(PE_EV_ACCEPT, NULL);
    enqueue(PE_EV_PS_RDY, NULL);
    inject_reset(0u);
    pe_event_t ev; pe_caps_t c;
    CHECK(phy_pop(&ev, &c) && ev == PE_EV_HARD_RESET_RX);
    CHECK(!phy_pop(&ev, &c));
    CHECK(phy_hard_reset_seen());
    return true;
}
static bool reset_wins_even_if_queue_was_full(void) {
    reset_fixture();
    for (unsigned i = 0; i < Q_LEN - 1u; ++i) enqueue(PE_EV_CAPS, &caps6);
    CHECK(lost == 0u);
    inject_reset(0u);
    pe_event_t ev; pe_caps_t c;
    CHECK(phy_pop(&ev, &c) && ev == PE_EV_HARD_RESET_RX);
    CHECK(!phy_pop(&ev, &c));
    return true;
}
static bool reset_cancels_retry_and_protocol_counters(void) {
    reset_fixture();
    last_rx_valid = 1u; last_rx_id = 5u;
    tx_msg_id = 4u;
    CHECK(phy_send(PE_CMD_REQUEST, 0x6302bc14u, 10u));
    USBPD->STATUS = IF_TX_END; USBPD_IRQHandler(); USBPD->STATUS = 0;
    CHECK(tx_wait_crc && crc_wait_armed);
    retry_count = 1u;
    inject_reset(0u);
    CHECK(hard_reset && !tx_wait_crc && !crc_wait_armed);
    CHECK(tx_msg_id == 0u && tx_expected_id == 0u);
    CHECK(!last_rx_valid && last_rx_id == 0u);
    CHECK(retry_count == 0u && retry_size == 0u && retry_eop_tick == 0u);
    SysTick->CNTL += CRC_RECEIVE_TICKS * 3u;
    phy_maintain(10000u);
    CHECK(retry_count == 0u && !phy_send(PE_CMD_REQUEST, 0x6302bc14u, 10001u));
    return true;
}
static bool reset_racing_with_phy_send_cannot_launch_a_tx(void) {
    reset_fixture();
    /* Begin a Request.  In the tiny interval preceding NVIC_DisableIRQ,
       a Hard Reset interrupt preempts the foreground. */
    inject_hr_before_irq_mask = true;
    CHECK(!phy_send(PE_CMD_REQUEST, 0x6302bc14u, 50u));
    CHECK(phy_hard_reset_seen());
    CHECK(!tx_wait_crc && retry_size == 0u);
    CHECK(!(USBPD->CONTROL & PD_TX_EN));
    pe_event_t ev; pe_caps_t c;
    CHECK(phy_pop(&ev, &c) && ev == PE_EV_HARD_RESET_RX);
    CHECK(!phy_pop(&ev, &c));
    return true;
}
static bool simultaneous_reset_rx_tx_interrupt_has_priority(void) {
    reset_fixture();
    CHECK(phy_send(PE_CMD_REQUEST, 0x6302bc14u, 0u));
    inject_reset(IF_RX_ACT | IF_TX_END);
    CHECK(hard_reset && !tx_wait_crc && !crc_wait_armed);
    pe_event_t ev; pe_caps_t c;
    CHECK(phy_pop(&ev, &c) && ev == PE_EV_HARD_RESET_RX);
    CHECK(!phy_pop(&ev, &c));
    return true;
}
static bool sop_messages_do_not_get_goodcrc_until_real_detach(void) {
    reset_fixture();
    inject_reset(0u);
    pe_event_t ev; pe_caps_t c;
    CHECK(phy_pop(&ev, &c) && ev == PE_EV_HARD_RESET_RX);
    uint8_t head_before = trace_head;
    inject_caps(0u);
    CHECK(trace_head == head_before); /* No GoodCRC and no new RX records */
    CHECK(!phy_pop(&ev, &c));
    CHECK(!tx_wait_crc);
    return true;
}
static bool cc_remains_attached_but_cannot_restart_on_elapsed_time(void) {
    reset_fixture();
    inject_reset(0u);
    phy_poll_cc(2000u);
    CHECK(phy_attached() && phy_hard_reset_seen());
    CHECK(!phy_send(PE_CMD_REQUEST, 0x6302bc14u, 2000u));
    pe_t p; pe_ready(&p);
    pe_event(&p, PE_EV_HARD_RESET_RX, NULL, 100u);
    pe_tick(&p, 999999u);
    CHECK(p.state == PE_RECOVERY && !p.contract_valid);
    return true;
}
static bool genuine_detach_rearm_allows_new_attach(void) {
    reset_fixture();
    inject_reset(0u);
    pe_event_t ev; pe_caps_t c;
    CHECK(phy_pop(&ev, &c) && ev == PE_EV_HARD_RESET_RX);
    USBPD->PORT_CC1 &= ~PA_CC_AI;
    phy_poll_cc(100u);
    phy_poll_cc(250u);
    CHECK(!phy_attached());
    CHECK(phy_pop(&ev, &c) && ev == PE_EV_DETACH);
    phy_reset_after_detach();
    CHECK(!phy_hard_reset_seen() && !phy_attached());
    USBPD->PORT_CC1 |= PA_CC_AI;
    phy_poll_cc(1000u); phy_poll_cc(1020u);
    CHECK(phy_attached() && !phy_hard_reset_seen());
    CHECK(phy_pop(&ev, &c) && ev == PE_EV_ATTACH);
    pe_t p; pe_init(&p);
    pe_event(&p, ev, NULL, 1020u);
    inject_caps(0u);
    CHECK(phy_pop(&ev, &c) && ev == PE_EV_CAPS);
    pe_event(&p, ev, &c, 1030u);
    CHECK(p.state == PE_WAIT_ACCEPT && p.command == PE_CMD_REQUEST);
    return true;
}

static unsigned pass, fail;
static void run(const char *name, bool (*f)(void)) {
    if (f()) { ++pass; printf("PASS %s\n", name); }
    else { ++fail; printf("FAIL %s\n", name); }
}
int main(void) {
    run("policy clears all contract data", policy_clears_every_contract_field);
    run("unsent Request aborted", policy_aborts_pretransmission_request);
    run("stale PS_RDY cannot revalidate contract", policy_cannot_accept_stale_psrdy);
    run("Hard Reset flushes queued RX events", reset_discards_stale_rx_queue);
    run("Hard Reset delivered with full queue", reset_wins_even_if_queue_was_full);
    run("Hard Reset cancels PHY retries / resets IDs", reset_cancels_retry_and_protocol_counters);
    run("Hard Reset racing with phy_send cannot start TX", reset_racing_with_phy_send_cannot_launch_a_tx);
    run("Hard Reset wins simultaneous status bits", simultaneous_reset_rx_tx_interrupt_has_priority);
    run("no GoodCRC/new negotiation during Hard Reset", sop_messages_do_not_get_goodcrc_until_real_detach);
    run("no timer/CC-only auto restart", cc_remains_attached_but_cannot_restart_on_elapsed_time);
    run("physical detach/re-attach rearms PHY", genuine_detach_rearm_allows_new_attach);
    printf("M13a Host: %u PASS, %u FAIL\n", pass, fail);
    return fail ? 1 : 0;
}
