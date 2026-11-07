#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "pd_vbus_monitor.h"

static bool mock_end;
static bool mock_start_fails;
static uint16_t mock_raw;
static unsigned starts;

bool adc_voltage_start_async(void)
{
    starts++;
    mock_end = false;
    return !mock_start_fails;
}
bool adc_voltage_poll_async(uint16_t *raw)
{
    if (!mock_end) return false;
    *raw = mock_raw;
    mock_end = false;
    return true;
}
uint32_t adc_voltage_vbus_mv(uint16_t raw) { return (uint32_t)raw * 11u; }

static void reset_test(void)
{
    mock_end = false;
    mock_start_fails = false;
    mock_raw = 790;
    starts = 0;
    pd_vbus_monitor_clear();
}
static void complete(uint32_t time_ms, uint16_t value, bool attached,
                     bool ready, bool reset)
{
    mock_raw = value;
    mock_end = true;
    pd_vbus_monitor_tick(time_ms, attached, ready, reset);
}

int main(void)
{
    pd_vbus_point_t p;
    const pd_vbus_history_t *h;
    reset_test();
    pd_vbus_monitor_tick(100, true, true, false); /* unattached monitor off */
    assert(starts == 0);
    pd_vbus_monitor_attach(100);
    pd_vbus_monitor_tick(100, true, true, false);
    assert(starts == 1 && pd_vbus_monitor_busy());
    pd_vbus_monitor_tick(101, true, true, false);
    assert(pd_vbus_monitor_busy()); /* ADC EOC not ready: no spinning */
    complete(102, 789, true, true, false);
    h = pd_vbus_monitor_history();
    assert(!pd_vbus_monitor_busy() && h->count == 1);
    assert(pd_vbus_history_get(h, 0, &p) && p.time_ms == 102 && p.adc_raw == 789);
    pd_vbus_monitor_tick(109, true, true, false);
    assert(starts == 1);
    pd_vbus_monitor_tick(110, true, true, false);
    assert(starts == 2);
    complete(111, 791, true, false, false);
    assert(pd_vbus_history_get(h, 1, &p) && (p.flags & PD_VBUS_FLAG_READY) != 0u);
    /* Flags describe the state when the conversion started, not when EOC came. */
    pd_vbus_monitor_tick(120, true, false, true);
    complete(121, 100, true, false, true);
    assert(pd_vbus_history_get(h, 2, &p) && (p.flags & PD_VBUS_FLAG_HARD_RESET));
    pd_vbus_monitor_detach(130);
    pd_vbus_monitor_tick(130, false, false, true);
    complete(131, 15, false, false, true);
    assert(pd_vbus_history_get(h, 3, &p) && !(p.flags & PD_VBUS_FLAG_ATTACHED));
    pd_vbus_monitor_stop();
    pd_vbus_monitor_tick(140, false, false, true);
    assert(starts == 4);

    /* 10ms rate during a long run, oldest data overwritten, newest retained. */
    reset_test();
    pd_vbus_monitor_attach(0);
    for (uint32_t i = 0; i < 200; i++) {
        pd_vbus_monitor_tick(i * 10, true, true, false);
        complete(i * 10 + 1, (uint16_t)(600 + i), true, true, false);
    }
    h = pd_vbus_monitor_history();
    assert(starts == 200 && h->count == 128 && h->overwritten == 72);
    assert(h->min_ready_raw == 600 && h->max_ready_raw == 799);
    assert(pd_vbus_history_get(h, 0, &p) && p.adc_raw == 672);
    assert(pd_vbus_history_get(h, 127, &p) && p.adc_raw == 799);

    /* A physical pull precedes the debounced CC detach. Retain both the
     * high pre-pull plateau and the low post-pull samples in the final ring. */
    reset_test();
    pd_vbus_monitor_attach(0);
    for (uint32_t t = 0; t < 1650; t += 10) {
        if (t == 1150) pd_vbus_monitor_detach(t);
        bool cc = t < 1150;
        uint16_t v = t < 1000 ? 791u : (t < 1030 ? 280u : 15u);
        pd_vbus_monitor_tick(t, cc, cc, false);
        complete(t + 1, v, cc, cc, false);
    }
    pd_vbus_monitor_stop();
    h = pd_vbus_monitor_history();
    assert(h->count == PD_VBUS_HISTORY_LEN);
    bool saw_high = false, saw_low_during_debounce = false, saw_low_detached = false;
    for (uint16_t i = 0; i < h->count; ++i) {
        assert(pd_vbus_history_get(h, i, &p));
        if (p.adc_raw == 791) saw_high = true;
        if (p.time_ms >= 1031 && p.time_ms < 1150 && p.adc_raw == 15 &&
            (p.flags & PD_VBUS_FLAG_ATTACHED)) saw_low_during_debounce = true;
        if (p.time_ms >= 1150 && p.adc_raw == 15 &&
            !(p.flags & PD_VBUS_FLAG_ATTACHED)) saw_low_detached = true;
    }
    assert(saw_high && saw_low_during_debounce && saw_low_detached);

    /* 32bit now_ms wraps, but scheduling and ordering still work. */
    reset_test();
    pd_vbus_monitor_attach(0xfffffffau);
    pd_vbus_monitor_tick(0xfffffffau, true, false, false);
    complete(0xfffffffbu, 800, true, false, false);
    pd_vbus_monitor_tick(3, true, false, false);
    assert(starts == 1);
    pd_vbus_monitor_tick(4, true, false, false);
    assert(starts == 2);
    complete(5, 801, true, false, false);
    assert(pd_vbus_monitor_history()->count == 2);

    /* Timeout must fail closed: do not continuously re-arm bad ADC. */
    reset_test();
    pd_vbus_monitor_attach(0);
    pd_vbus_monitor_tick(0, true, true, false);
    pd_vbus_monitor_tick(2, true, true, false);
    assert(!pd_vbus_monitor_stopped_on_error());
    pd_vbus_monitor_tick(3, true, true, false);
    assert(pd_vbus_monitor_stopped_on_error() && pd_vbus_monitor_timeouts() == 1);
    pd_vbus_monitor_tick(99, true, true, false);
    assert(starts == 1);
    reset_test();
    mock_start_fails = true;
    pd_vbus_monitor_attach(0);
    pd_vbus_monitor_tick(0, true, true, false);
    assert(pd_vbus_monitor_stopped_on_error() && starts == 1);

    /* One line per report_step; no asynchronous or IRQ-driven output. */
    reset_test();
    pd_vbus_monitor_attach(0);
    pd_vbus_monitor_tick(0, true, true, false);
    complete(1, 790, true, true, false);
    pd_vbus_monitor_detach(2);
    pd_vbus_monitor_stop();
    pd_vbus_monitor_report_begin();
    assert(!pd_vbus_monitor_report_step()); /* BEGIN */
    assert(!pd_vbus_monitor_report_step()); /* one point */
    assert(pd_vbus_monitor_report_step());  /* END */
    puts("M13a-2 async VBUS monitor: PASS");
    return 0;
}
