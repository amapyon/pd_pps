/* M12b: execute the ACTUAL production PHY state machine with register mocks.
 * No hardware timer, charger, UART or USB-PD line is driven by this test.
 * Build: gcc -std=c11 -Wall -Wextra -Werror -Itests/mocks -Isrc ...
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
void NVIC_DisableIRQ(int irq) { assert(irq == USBPD_IRQn); irq_enabled = false; }
void NVIC_EnableIRQ(int irq) { assert(irq == USBPD_IRQn); irq_enabled = true; }
void Delay_Us(uint32_t delay) { (void)delay; }

/* Source is intentionally compiled here unchanged, to allow white-box
 * assertions on the actual driver state and packed retransmit frame. */
#include "../src/pd_phy_ch32x035.c"

static unsigned tests_run;
#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL at %s:%d: %s\n", __FILE__, __LINE__, #c); \
    return false; } } while (0)

static void fixture(uint8_t cc) {
    memset(&mock_systick, 0, sizeof(mock_systick));
    memset(&mock_rcc, 0, sizeof(mock_rcc));
    memset(&mock_gpioc, 0, sizeof(mock_gpioc));
    memset(&mock_afio, 0, sizeof(mock_afio));
    memset(&mock_usbpd, 0, sizeof(mock_usbpd));
    irq_enabled = false;
    phy_init();
    trace_head = trace_tail = 0;
    trace_drops = 0;
    if (cc == 1) mock_usbpd.PORT_CC1 |= PA_CC_AI;
    else mock_usbpd.PORT_CC2 |= PA_CC_AI;
    phy_poll_cc(1u);
    phy_poll_cc(21u);
    assert(phy_attached() && selected_cc == cc && irq_enabled);
    pe_event_t ev; pe_caps_t caps;
    assert(phy_pop(&ev, &caps) && ev == PE_EV_ATTACH);
    assert(!phy_pop(&ev, &caps));
}

static void tx_end(uint32_t tick) {
    SysTick->CNTL = tick;
    /* Hardware TX_END means the last EOP bit has left the PHY. */
    USBPD->STATUS = IF_TX_END;
    USBPD_IRQHandler();
    USBPD->STATUS = 0u; /* Model hardware W1C behavior outside the driver. */
}

static void rx_goodcrc(uint32_t tick, uint8_t mid) {
    SysTick->CNTL = tick;
    uint16_t header = hdr(PD_CTRL_GOODCRC, 0u, mid);
    memcpy(dma_buffer, &header, 2u);
    USBPD->BMC_BYTE_CNT = 6u; /* Header (2) + CRC (4), as seen by CH32. */
    USBPD->STATUS = IF_RX_ACT | BMC_AUX_SOP0;
    USBPD_IRQHandler();
    USBPD->STATUS = 0u;
}

static bool no_retry_before_tx_end(void) {
    fixture(1);
    CHECK(phy_send(PE_CMD_REQUEST, 0x6302bc14u, 0u));
    CHECK(tx_wait_crc && !crc_wait_armed && retry_count == 0u);
    CHECK(USBPD->CONTROL & PD_TX_EN);
    SysTick->CNTL = 1000000u;
    phy_maintain(3000u); /* Even a very old wall-clock cannot start retry. */
    CHECK(retry_count == 0u && tx_wait_crc && !crc_wait_armed);
    return true;
}

static bool timeouts_preserve_mid_then_notify_failure(void) {
    fixture(2);
    const uint32_t rdo = 0x6302bc14u;
    const uint32_t t0 = 0xffff0000u; /* Test unsigned 32-bit SysTick wrap. */
    CHECK(phy_send(PE_CMD_REQUEST, rdo, 0u));
    uint8_t original[6]; memcpy(original, retry_frame, 6u);
    CHECK(tx_msg_id == 0u && tx_expected_id == 0u && retry_count == 0u);
    CHECK(retry_size == 6u);
    tx_end(t0);
    CHECK(crc_wait_armed && retry_eop_tick == t0);
    SysTick->CNTL = t0 + CRC_RECEIVE_TICKS - 1u;
    phy_maintain(0u);
    CHECK(retry_count == 0u && !(USBPD->CONTROL & PD_TX_EN));
    SysTick->CNTL++;
    phy_maintain(0u);
    CHECK(retry_count == 1u && tx_msg_id == 0u && tx_wait_crc);
    CHECK(!crc_wait_armed && (USBPD->CONTROL & PD_TX_EN));
    CHECK(memcmp(original, retry_frame, 6u) == 0);
    CHECK(memcmp(original, dma_buffer, 6u) == 0);
    /* No fresh timeout until this retry finishes physically. */
    SysTick->CNTL += CRC_RECEIVE_TICKS * 4u;
    phy_maintain(5000u);
    CHECK(retry_count == 1u && !crc_wait_armed);
    uint32_t t1 = SysTick->CNTL + 1000u;
    tx_end(t1);
    SysTick->CNTL = t1 + CRC_RECEIVE_TICKS - 1u;
    phy_maintain(0u);
    CHECK(retry_count == 1u);
    SysTick->CNTL++;
    phy_maintain(0u);
    CHECK(retry_count == 2u && tx_msg_id == 0u && !crc_wait_armed);
    CHECK(memcmp(original, dma_buffer, 6u) == 0);
    uint32_t t2 = SysTick->CNTL + 1000u;
    tx_end(t2);
    SysTick->CNTL = t2 + CRC_RECEIVE_TICKS - 1u;
    phy_maintain(0u);
    CHECK(tx_wait_crc && retry_count == 2u);
    SysTick->CNTL++;
    phy_maintain(0u);
    CHECK(!tx_wait_crc && !crc_wait_armed);
    /* USB PD 3.2 §9.1.2.2.1.9 requires an increment on terminal failure. */
    CHECK(tx_msg_id == 1u);
    pe_event_t ev; pe_caps_t caps;
    CHECK(phy_pop(&ev, &caps) && ev == PE_EV_TX_FAILED);
    CHECK(!phy_pop(&ev, &caps));
    SysTick->CNTL += CRC_RECEIVE_TICKS * 3u;
    phy_maintain(20000u);
    CHECK(!tx_wait_crc && retry_count == 2u);
    /* Policy must invalidate any previous contract on terminal failure. */
    pe_t pe; pe_init(&pe);
    pe_event(&pe, PE_EV_ATTACH, NULL, 0u);
    pe_caps_t source = {.pdo={0x0801912cu,0x0002d12cu,0x0003c12cu,
                             0x0004b12cu,0x0006412cu,0xc1a4213cu},.count=6u,.msg_id=0u};
    pe_event(&pe, PE_EV_CAPS, &source, 5u);
    pe_command_result(&pe, true, 5u);
    pe_event(&pe, PE_EV_ACCEPT, NULL, 6u);
    pe_event(&pe, PE_EV_PS_RDY, NULL, 10u);
    CHECK(pe.state == PE_READY && pe.contract_valid);
    pe_event(&pe, ev, NULL, 100u);
    CHECK(pe.state == PE_RECOVERY && !pe.contract_valid && !pe.pps_active);
    CHECK(pe.command == PE_CMD_NONE && pe.faults == 1u);
    return true;
}

static bool matching_goodcrc_stops_timer_and_advances_id(void) {
    fixture(1);
    CHECK(phy_send(PE_CMD_GET_SOURCE_CAP, 0u, 0u));
    tx_end(1000u);
    rx_goodcrc(20000u, 0u);
    CHECK(!tx_wait_crc && !crc_wait_armed && tx_msg_id == 1u);
    SysTick->CNTL = 9000000u;
    phy_maintain(20000u);
    CHECK(retry_count == 0u && !tx_wait_crc);
    CHECK(phy_send(PE_CMD_REQUEST, 0x6302bc14u, 1u));
    uint16_t header; memcpy(&header, dma_buffer, 2u);
    CHECK(((header >> 9) & 7u) == 1u);
    tx_end(9010000u);
    rx_goodcrc(9020000u, 1u);
    CHECK(tx_msg_id == 2u && !tx_wait_crc);
    return true;
}

static bool mismatched_goodcrc_does_not_cancel_retry(void) {
    fixture(1);
    CHECK(phy_send(PE_CMD_REQUEST, 0x6302bc14u, 0u));
    tx_end(1000u);
    rx_goodcrc(15000u, 7u); /* Valid GoodCRC format but wrong MessageID. */
    CHECK(tx_msg_id == 0u && tx_wait_crc && crc_wait_armed);
    SysTick->CNTL = 1000u + CRC_RECEIVE_TICKS - 1u;
    phy_maintain(0u);
    CHECK(retry_count == 0u);
    SysTick->CNTL++;
    phy_maintain(0u);
    CHECK(retry_count == 1u && tx_msg_id == 0u);
    tx_end(70000u);
    rx_goodcrc(80000u, 0u);
    CHECK(!tx_wait_crc && !crc_wait_armed && tx_msg_id == 1u);
    SysTick->CNTL += CRC_RECEIVE_TICKS * 4u;
    phy_maintain(999u);
    CHECK(retry_count == 0u);
    return true;
}

static bool msg_id_rollover_and_reset_cancel_timeout(void) {
    fixture(2);
    tx_msg_id = 7u;
    CHECK(phy_send(PE_CMD_REQUEST, 0x6302bc14u, 0u));
    uint16_t header; memcpy(&header, dma_buffer, 2u);
    CHECK(((header >> 9) & 7u) == 7u);
    tx_end(100u);
    rx_goodcrc(2000u, 7u);
    CHECK(tx_msg_id == 0u && !tx_wait_crc);
    CHECK(phy_send(PE_CMD_REQUEST, 0x6302bc14u, 0u));
    tx_end(10000u);
    USBPD->STATUS = IF_RX_RESET;
    USBPD_IRQHandler();
    USBPD->STATUS = 0;
    CHECK(hard_reset && !tx_wait_crc && !crc_wait_armed && tx_msg_id == 0u);
    SysTick->CNTL = 10000u + CRC_RECEIVE_TICKS * 3u;
    phy_maintain(999u);
    CHECK(retry_count == 0u && !phy_send(PE_CMD_REQUEST, 0u, 0u));
    pe_event_t ev; pe_caps_t caps;
    CHECK(phy_pop(&ev, &caps) && ev == PE_EV_HARD_RESET_RX);
    return true;
}

static bool detach_stops_maintenance(void) {
    fixture(1);
    CHECK(phy_send(PE_CMD_REQUEST, 0x6302bc14u, 0u));
    tx_end(1000u);
    mock_usbpd.PORT_CC1 &= ~PA_CC_AI;
    phy_poll_cc(100u); /* first lost-CC sample */
    CHECK(phy_attached());
    phy_poll_cc(249u); /* still below 150ms debounce */
    CHECK(phy_attached());
    phy_poll_cc(250u);
    CHECK(!phy_attached());
    SysTick->CNTL = 1000u + CRC_RECEIVE_TICKS * 5u;
    phy_maintain(300u);
    CHECK(retry_count == 0u);
    pe_event_t ev; pe_caps_t caps;
    CHECK(phy_pop(&ev, &caps) && ev == PE_EV_DETACH);
    return true;
}

static bool run(const char *name, bool (*fn)(void)) {
    bool ok = fn();
    if (ok) { ++tests_run; printf("PASS %s\n",name); }
    return ok;
}
int main(void) {
    if (!run("no retry before TX_END", no_retry_before_tx_end)) return 1;
    if (!run("1000us timeout / 2 retries / TX_FAILED / PE fail-closed / wrap", timeouts_preserve_mid_then_notify_failure)) return 1;
    if (!run("valid GoodCRC stops timer / next MID", matching_goodcrc_stops_timer_and_advances_id)) return 1;
    if (!run("wrong MID GoodCRC cannot stop timer", mismatched_goodcrc_does_not_cancel_retry)) return 1;
    if (!run("MID wraps 7->0 / Hard Reset stops timer", msg_id_rollover_and_reset_cancel_timeout)) return 1;
    if (!run("CC detach stops retry", detach_stops_maintenance)) return 1;
    printf("PASS ALL: %u M12b host PHY tests\n", tests_run);
    return 0;
}
