/* M13e HOST-ONLY fault injection: real ADC monitor / PE / PHY / stop diary.
 * No injection flags or deliberately broken paths are compiled into firmware.
 * Drives the same fail-closed branch ordering as src/main.c. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ch32fun.h"
#include "pd_policy.h"
#include "pd_vbus_monitor.h"
#include "pd_stop_diag.h"
#include "pd_stability.h"

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

/* Stub only the ADC hardware driver. The monitor and its 3ms timeout logic
 * below are the actual production modules. */
static bool adc_start_succeeds;
static bool adc_complete;
static uint32_t adc_starts;
bool adc_voltage_start_async(void) {
    ++adc_starts;
    return adc_start_succeeds;
}
bool adc_voltage_poll_async(uint16_t *raw) {
    if (!adc_complete) return false;
    adc_complete = false;
    *raw = 790u;
    return true;
}
uint32_t adc_voltage_vbus_mv(uint16_t raw) { return (uint32_t)raw * 9u; }

static pe_t policy;
static pd_stop_diag_t diag;
static pd_stability_t stats;
static const pe_caps_t caps = { .pdo = {
    0x0a01912cu, 0x0002d0deu, 0x0003c0a7u, 0xc0dc3224u
}, .count = 4u };

static void begin_session(uint32_t now) {
    memset(&mock_systick, 0, sizeof(mock_systick));
    memset(&mock_rcc, 0, sizeof(mock_rcc));
    memset(&mock_gpioc, 0, sizeof(mock_gpioc));
    memset(&mock_afio, 0, sizeof(mock_afio));
    memset(&mock_usbpd, 0, sizeof(mock_usbpd));
    adc_start_succeeds = true;
    adc_complete = false;
    adc_starts = 0u;
    pe_init(&policy);
    pd_stop_diag_init(&diag);
    pd_stability_init(&stats);
    pd_vbus_monitor_init();
    phy_init();
    USBPD->PORT_CC1 = PA_CC_AI;
    phy_poll_cc(now - 25u);
    phy_poll_cc(now);
    assert(phy_attached() && irq_enabled);
    pe_event_t ev;
    pe_caps_t ignored;
    assert(phy_pop(&ev, &ignored) && ev == PE_EV_ATTACH);
    pe_event(&policy, ev, NULL, now);
    pd_stop_diag_attach(&diag, now);
    pd_stability_attach(&stats, now, phy_trace_dropped(), phy_lost_events());
    pd_vbus_monitor_attach(now);
    assert(policy.state == PE_WAIT_CAPS);
}
static void negotiate_ready(uint32_t t) {
    assert(policy.state == PE_WAIT_CAPS);
    pe_event(&policy, PE_EV_CAPS, &caps, t);
    assert(policy.state == PE_WAIT_ACCEPT && policy.command == PE_CMD_REQUEST);
    pe_command_result(&policy, true, t + 1u);
    pe_event(&policy, PE_EV_ACCEPT, NULL, t + 2u);
    pe_event(&policy, PE_EV_PS_RDY, NULL, t + 4u);
    assert(policy.state == PE_READY && policy.contract_valid && policy.pps_active);
}
static void assert_fail_closed(void) {
    assert(phy_attached() && phy_vbus_loss_latched());
    assert(!phy_hard_reset_seen());
    assert(policy.state == PE_RECOVERY);
    assert(!policy.contract_valid && !policy.pps_active);
    assert(policy.command == PE_CMD_NONE);
    assert(!tx_wait_crc && !crc_wait_armed && retry_count == 0u);
    assert(!phy_send(PE_CMD_REQUEST, 0x4302bc14u, 600u));
    /* Inject valid SOP Accept: a latched failure must not GoodCRC it. */
    uint16_t h = hdr(PD_CTRL_ACCEPT, 0u, 4u);
    memcpy(dma_buffer, &h, 2u);
    USBPD->BMC_BYTE_CNT = 6u;
    USBPD->STATUS = IF_RX_ACT | BMC_AUX_SOP0;
    USBPD_IRQHandler();
    USBPD->STATUS = 0u; /* emulate W1C on the host */
    pe_event_t ev;
    pe_caps_t ignored;
    assert(!phy_pop(&ev, &ignored));
    assert(!tx_wait_crc && !crc_wait_armed);
    pd_trace_t trace;
    while (phy_trace_pop(&trace)) assert(trace.kind != PDTR_TX_GOODCRC);
    /* Regaining a notional VBUS voltage alone must not release the latch. */
    pe_event(&policy, PE_EV_CAPS, &caps, 900u);
    pe_event(&policy, PE_EV_PS_RDY, NULL, 901u);
    pe_tick(&policy, 1000u);
    assert(policy.state == PE_RECOVERY && !policy.contract_valid);
    assert(!phy_send(PE_CMD_REQUEST, 0x4302bc14u, 1001u));
}
static void detach_and_reattach(uint32_t now, pd_stop_reason_t expected) {
    uint32_t old_elapsed = diag.stop_rel_ms;
    uint32_t old_adc_faults = stats.adc_fault;
    assert(diag.found && diag.reason == (uint8_t)expected);
    USBPD->PORT_CC1 = 0u;
    phy_poll_cc(now);
    phy_poll_cc(now + 160u);
    assert(!phy_attached());
    pe_event_t ev;
    pe_caps_t ignored;
    assert(phy_pop(&ev, &ignored) && ev == PE_EV_DETACH);
    pd_stop_diag_detach(&diag, now + 160u);
    pd_stability_detach(&stats, now + 160u, phy_trace_dropped(),
                        phy_lost_events(), policy.faults);
    pe_event(&policy, ev, NULL, now + 160u);
    pd_vbus_monitor_detach(now + 160u);
    assert(policy.state == PE_DETACHED);
    assert(diag.detach_seen && !diag.active);
    assert(diag.detach_after_stop_ms == (now + 160u - diag.started_ms) - old_elapsed);
    assert(stats.adc_fault == old_adc_faults && !stats.active);
    phy_reset_after_detach();
    assert(!phy_vbus_loss_latched());
    USBPD->PORT_CC1 = PA_CC_AI;
    phy_poll_cc(now + 180u);
    phy_poll_cc(now + 205u);
    assert(phy_attached() && irq_enabled);
    assert(phy_pop(&ev, &ignored) && ev == PE_EV_ATTACH);
    pe_event(&policy, ev, NULL, now + 205u);
    pd_vbus_monitor_attach(now + 205u);
    pd_stop_diag_attach(&diag, now + 205u);
    pd_stability_attach(&stats, now + 205u, phy_trace_dropped(), phy_lost_events());
    assert(policy.state == PE_WAIT_CAPS && !diag.found && diag.reason == PD_STOP_NONE);
    assert(!pd_vbus_monitor_stopped_on_error());
    assert(stats.adc_fault == 0u && stats.recovery_entries == 0u);
    negotiate_ready(now + 210u);
    /* The reattached PE/PHY can initiate a Request again. */
    assert(phy_send(PE_CMD_REQUEST, policy.rdo, now + 220u));
    assert(tx_wait_crc);
}
static void monitor_fault_case(bool start_failure) {
    const uint32_t session = 25u;
    begin_session(session);
    negotiate_ready(30u);
    /* Put a PD transmission in flight, to check abort clears retries. */
    assert(phy_send(PE_CMD_REQUEST, policy.rdo, 35u));
    assert(tx_wait_crc);
    adc_start_succeeds = !start_failure;
    pd_vbus_monitor_tick(40u, true, true, false);
    assert(adc_starts == 1u);
    if (!start_failure) {
        assert(pd_vbus_monitor_busy());
        pd_vbus_monitor_tick(42u, true, true, false);
        assert(!pd_vbus_monitor_stopped_on_error());
        pd_vbus_monitor_tick(43u, true, true, false);
    }
    assert(pd_vbus_monitor_stopped_on_error());
    assert(pd_vbus_monitor_timeouts() == 1u);
    assert(!pd_vbus_monitor_loss_detected());
    uint32_t when = start_failure ? 40u : 43u;
    pe_state_t before = policy.state;
    assert(phy_abort_for_adc_failure());
    pe_event(&policy, PE_EV_SENSOR_FAULT, NULL, when);
    pd_stop_diag_note(&diag, PD_STOP_ADC_MONITOR, when, before,
                      policy.state, (uint16_t)pd_vbus_monitor_timeouts());
    pd_stability_event(&stats, PE_EV_SENSOR_FAULT, before, &policy);
    assert(policy.faults == 1u && stats.adc_fault == 1u && stats.recovery_entries == 1u);
    assert(diag.reason == PD_STOP_ADC_MONITOR && diag.aux == 1u);
    assert(diag.stop_rel_ms == when - session);
    assert(!phy_abort_for_adc_failure());
    pd_vbus_monitor_tick(100u, true, false, false);
    assert(adc_starts == 1u); /* no ADC rearm after failure */
    pd_stop_diag_note(&diag, PD_STOP_PE_TIMEOUT_ACCEPT, 110u,
                      PE_WAIT_ACCEPT, PE_RECOVERY, 0u);
    assert(diag.reason == PD_STOP_ADC_MONITOR); /* first failure wins */
    assert_fail_closed();
    detach_and_reattach(1200u, PD_STOP_ADC_MONITOR);
}
static void timeout_fault_case(unsigned scenario) {
    const uint32_t session = 25u;
    begin_session(session);
    uint32_t when;
    pe_state_t target;
    pd_stop_reason_t reason;
    if (scenario == 0u) {
        /* SourceCaps absent after attach. */
        target = PE_WAIT_CAPS; reason = PD_STOP_PE_TIMEOUT_CAPS;
        when = 675u;
    } else if (scenario == 1u) {
        /* SourceCaps present, but Request not sent before 15ms start limit. */
        pe_event(&policy, PE_EV_CAPS, &caps, 30u);
        assert(!policy.request_sent && policy.command == PE_CMD_REQUEST);
        target = PE_WAIT_ACCEPT; reason = PD_STOP_PE_TIMEOUT_ACCEPT;
        when = 45u;
    } else if (scenario == 2u) {
        /* Initial Request was sent but Accept never arrived. */
        pe_event(&policy, PE_EV_CAPS, &caps, 30u);
        pe_command_result(&policy, true, 31u);
        target = PE_WAIT_ACCEPT; reason = PD_STOP_PE_TIMEOUT_ACCEPT;
        when = 76u;
    } else if (scenario == 3u) {
        /* Accept received, but PS_RDY never arrived. */
        pe_event(&policy, PE_EV_CAPS, &caps, 30u);
        pe_command_result(&policy, true, 31u);
        pe_event(&policy, PE_EV_ACCEPT, NULL, 32u);
        target = PE_WAIT_PS_RDY; reason = PD_STOP_PE_TIMEOUT_PS_RDY;
        when = 532u;
    } else if (scenario == 4u) {
        /* PPS renewal after a genuine Ready: deadline must kill old contract. */
        negotiate_ready(30u);
        pe_tick(&policy, 5035u);
        assert(policy.state == PE_WAIT_ACCEPT && policy.command == PE_CMD_REQUEST);
        pd_stability_refresh_started(&stats);
        pe_command_result(&policy, true, 5036u);
        target = PE_WAIT_ACCEPT; reason = PD_STOP_PE_TIMEOUT_ACCEPT;
        when = 5081u;
    } else {
        /* TX_FAILED is event driven, not a timer. */
        pe_event(&policy, PE_EV_CAPS, &caps, 30u);
        target = PE_WAIT_ACCEPT; reason = PD_STOP_PE_TX_FAILED;
        when = 32u;
    }
    assert(policy.state == target);
    pe_state_t before = policy.state;
    if (scenario == 5u) {
        pe_event(&policy, PE_EV_TX_FAILED, NULL, when);
        pd_stop_diag_note(&diag, pd_stop_diag_pe_event_reason(PE_EV_TX_FAILED),
                          when, before, policy.state, (uint16_t)PE_EV_TX_FAILED);
    } else {
        pe_tick(&policy, when - 1u);
        assert(policy.state == target); /* exactly before deadline */
        pe_tick(&policy, when);
        pd_stop_diag_note(&diag, pd_stop_diag_pe_tick_reason(before),
                          when, before, policy.state, 0u);
    }
    pd_stability_event(&stats, PE_EV_TX_FAILED, before, &policy);
    assert(policy.state == PE_RECOVERY && !policy.contract_valid);
    assert(policy.command == PE_CMD_NONE && policy.faults == 1u);
    assert(phy_abort_for_policy_failure((uint16_t)before));
    assert(!phy_abort_for_policy_failure((uint16_t)before));
    assert(stats.recovery_entries == 1u && stats.adc_fault == 0u);
    assert(diag.reason == (uint8_t)reason && diag.pe_before == before);
    assert(diag.pe_after == PE_RECOVERY && diag.stop_rel_ms == when - session);
    assert_fail_closed();
    detach_and_reattach(1200u, reason);
}
int main(void) {
    monitor_fault_case(false); puts("M13e ADC conversion stalled: PASS");
    monitor_fault_case(true);  puts("M13e ADC conversion start failure: PASS");
    for (unsigned i = 0; i < 6u; ++i) {
        timeout_fault_case(i);
        printf("M13e PE timeout/TX failure case %u: PASS\n", i);
    }
    puts("M13e all 8 fail-closed, no GoodCRC, physical reattach recovery: PASS");
    return 0;
}
