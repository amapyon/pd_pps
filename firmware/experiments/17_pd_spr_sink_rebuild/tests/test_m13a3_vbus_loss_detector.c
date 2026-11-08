/* Actual VBUS monitor logic, driven with a stub asynchronous ADC. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "pd_vbus_monitor.h"

static uint16_t adc_raw;
static bool adc_eoc;
static unsigned starts;
bool adc_voltage_start_async(void) { starts++; return true; }
bool adc_voltage_poll_async(uint16_t *raw) {
    if (!adc_eoc) return false;
    adc_eoc = false;
    *raw = adc_raw;
    return true;
}
uint32_t adc_voltage_vbus_mv(uint16_t raw) { return (uint32_t)raw * 9u; }
static void fixture(uint32_t now) {
    adc_eoc = false; adc_raw = 0; starts = 0;
    pd_vbus_monitor_init();
    pd_vbus_monitor_attach(now);
}
static void sample(uint32_t at_ms, uint16_t raw, bool attached,
                   bool ready, bool hard_reset) {
    pd_vbus_monitor_tick(at_ms, attached, ready, hard_reset);
    assert(pd_vbus_monitor_busy());
    adc_raw = raw; adc_eoc = true;
    pd_vbus_monitor_tick(at_ms + 1u, attached, ready, hard_reset);
    assert(!pd_vbus_monitor_busy());
}
int main(void) {
    /* Input trace mirrors observed 7V -> 150mV, while CC remains attached
       and PE remains READY during the 150ms CC detach debounce. */
    fixture(0);
    sample(0, 792, true, true, false);
    sample(10, 791, true, true, false);
    sample(20, 19, true, true, false);
    assert(!pd_vbus_monitor_loss_detected());
    sample(30, 18, true, true, false);
    assert(!pd_vbus_monitor_loss_detected());
    sample(40, 17, true, true, false);
    assert(pd_vbus_monitor_loss_detected());
    assert(pd_vbus_monitor_loss_raw() == 17u);
    assert(pd_vbus_monitor_loss_time_ms() == 41u);
    sample(50, 792, true, true, false);
    assert(pd_vbus_monitor_loss_detected()); /* safety latch */
    pd_vbus_monitor_detach(100);
    sample(100, 18, false, false, false);
    assert(pd_vbus_monitor_loss_detected());
    pd_vbus_monitor_attach(200); /* new physical session clears latch */
    assert(!pd_vbus_monitor_loss_detected());

    /* A single low glitch must not stop the contract. The high sample
       resets the counter; two later lows are still insufficient. */
    fixture(0);
    sample(0, 17, true, true, false);
    sample(10, 792, true, true, false);
    sample(20, 18, true, true, false);
    sample(30, 18, true, true, false);
    assert(!pd_vbus_monitor_loss_detected());
    sample(40, 90, true, true, false); /* boundary inclusive */
    assert(pd_vbus_monitor_loss_detected());

    /* 91 raw (just over the chosen threshold) is not sufficient. */
    fixture(0);
    for (uint32_t i = 0; i < 6; i++) sample(i * 10u, 91, true, true, false);
    assert(!pd_vbus_monitor_loss_detected());

    /* Never use low voltage before an explicit READY contract. */
    fixture(0);
    for (uint32_t i = 0; i < 6; i++) sample(i * 10u, 15, true, false, false);
    assert(!pd_vbus_monitor_loss_detected());

    /* Hard Reset explicitly owns the VBUS drop/recovery path. A reset
       between start and ADC EOC must also discard a low candidate. */
    fixture(0);
    sample(0, 15, true, true, false);
    sample(10, 15, true, true, true);
    sample(20, 15, true, true, true);
    assert(!pd_vbus_monitor_loss_detected());
    pd_vbus_monitor_tick(30, true, true, false); /* start when ready */
    adc_raw = 15; adc_eoc = true;
    pd_vbus_monitor_tick(31, true, false, true); /* reset before EOC */
    assert(!pd_vbus_monitor_loss_detected());
    sample(40, 15, true, true, false);
    sample(50, 15, true, true, false);
    assert(!pd_vbus_monitor_loss_detected());

    /* No detection after CC is confirmed detached. */
    fixture(0);
    for (uint32_t i = 0; i < 4; i++) sample(i * 10u, 17, false, true, false);
    assert(!pd_vbus_monitor_loss_detected());

    /* Timestamp arithmetic and 10ms period survive 32-bit wrap. */
    fixture(0xfffffff0u);
    sample(0xfffffff0u, 18, true, true, false);
    sample(0xfffffffau, 18, true, true, false);
    sample(4u, 18, true, true, false);
    assert(pd_vbus_monitor_loss_detected());
    assert(pd_vbus_monitor_loss_time_ms() == 5u);

    /* No latch if ADC acquisition times out. Sensor health is separately
       reported; auto-recovery must never depend on timed-out data. */
    fixture(0);
    pd_vbus_monitor_tick(0, true, true, false);
    pd_vbus_monitor_tick(3, true, true, false);
    assert(pd_vbus_monitor_stopped_on_error());
    assert(!pd_vbus_monitor_loss_detected());
    puts("M13a-3 VBUS loss detector: PASS");
    return 0;
}
