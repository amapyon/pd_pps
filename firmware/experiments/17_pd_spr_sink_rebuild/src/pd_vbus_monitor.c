#include "pd_vbus_monitor.h"
#include "adc_voltage.h"
#include <stdio.h>

static pd_vbus_history_t history;
static pd_hr_vbus_t hr_observer;
static bool active;
static bool pending;
static bool failed;
static bool detached_seen;
static uint32_t detach_ms;
static uint32_t start_ms;
static uint32_t next_sample_ms;
static uint32_t timeouts;
static uint8_t pending_flags;
static uint16_t report_cursor;
static bool report_started;
static uint8_t low_streak;
static bool loss_detected;
static bool hr_autorecovered;
static bool hr_handoff;
static bool hr_pps_recovered;
static uint16_t loss_raw;
static uint32_t loss_time_ms;

void pd_vbus_monitor_clear(void)
{
    active = false;
    pending = false;
    failed = false;
    detached_seen = false;
    detach_ms = 0u;
    start_ms = 0u;
    next_sample_ms = 0u;
    timeouts = 0u;
    pending_flags = 0u;
    report_cursor = 0u;
    report_started = false;
    low_streak = 0u;
    loss_detected = false;
    hr_autorecovered = false;
    hr_handoff = false;
    hr_pps_recovered = false;
    loss_raw = 0u;
    loss_time_ms = 0u;
    pd_vbus_history_clear(&history);
    pd_hr_vbus_init(&hr_observer);
}
void pd_vbus_monitor_init(void) { pd_vbus_monitor_clear(); }

void pd_vbus_monitor_attach(uint32_t now_ms)
{
    pd_vbus_monitor_clear();
    active = true;
    next_sample_ms = now_ms;
}

void pd_vbus_monitor_detach(uint32_t now_ms)
{
    /* Keep sampling for 500ms after confirmed CC detach. Note that the
     * physical cable pull precedes CC_DETACH by ~150ms debounce. */
    if (!active) return;
    detached_seen = true;
    detach_ms = now_ms;
    pd_hr_vbus_detach(&hr_observer);
}

bool pd_vbus_monitor_busy(void) { return pending; }
void pd_vbus_monitor_hr_begin(uint32_t now_ms) {
    if (active && !detached_seen && !failed)
        pd_hr_vbus_begin(&hr_observer, now_ms);
    else if (active && failed) {
        pd_hr_vbus_begin(&hr_observer, now_ms);
        pd_hr_vbus_adc_failed(&hr_observer);
    }
}
const pd_hr_vbus_t *pd_vbus_monitor_hr_observation(void) { return &hr_observer; }
void pd_vbus_monitor_hr_handoff(void) { hr_handoff = true; }
void pd_vbus_monitor_hr_recovered(void) { hr_autorecovered = true; }
void pd_vbus_monitor_hr_contract_ready(bool pps) {
    if (!hr_handoff) return;
    hr_autorecovered = true;
    if (pps) hr_pps_recovered = true;
}

const pd_vbus_history_t *pd_vbus_monitor_history(void) { return &history; }
uint32_t pd_vbus_monitor_timeouts(void) { return timeouts; }
bool pd_vbus_monitor_stopped_on_error(void) { return failed; }
bool pd_vbus_monitor_loss_detected(void) { return loss_detected; }
uint16_t pd_vbus_monitor_loss_raw(void) { return loss_raw; }
uint32_t pd_vbus_monitor_loss_time_ms(void) { return loss_time_ms; }

void pd_vbus_monitor_tick(uint32_t now_ms, bool cc_attached,
                          bool ready, bool hard_reset)
{
    if (!active) return;
    /* The observer runs independently of PE/PHY recovery; sample failures
       and missed voltage transitions can only lead to diagnostic failure. */
    pd_hr_vbus_tick(&hr_observer, now_ms);
    if (failed) return;
    if (pending) {
        uint16_t raw;
        if (adc_voltage_poll_async(&raw)) {
            pending = false;
            pd_vbus_history_append(&history, now_ms, raw, pending_flags);
            /* Discard a conversion started before Hard Reset; it can be
               completed after the interrupt but still reflect old VBUS. */
            if (hard_reset && (pending_flags & PD_VBUS_FLAG_HARD_RESET))
                pd_hr_vbus_sample(&hr_observer, now_ms, raw);
            /* Only after a genuine negotiated READY contract: ignore 0V at
               initial attach, expected supply changes before PS_RDY, Hard
               Reset and any confirmed CC detach. Never act on a lone low
               conversion (noise, ADC mux settling, or timing jitter). */
            if (!loss_detected && cc_attached && !hard_reset &&
                (pending_flags & (PD_VBUS_FLAG_ATTACHED | PD_VBUS_FLAG_READY |
                                  PD_VBUS_FLAG_HARD_RESET)) ==
                    (PD_VBUS_FLAG_ATTACHED | PD_VBUS_FLAG_READY) &&
                ready && raw <= PD_VBUS_LOSS_RAW_MAX) {
                if (low_streak < PD_VBUS_LOSS_CONSECUTIVE) low_streak++;
                if (low_streak >= PD_VBUS_LOSS_CONSECUTIVE) {
                    loss_detected = true;
                    loss_raw = raw;
                    loss_time_ms = now_ms;
                }
            } else {
                low_streak = 0u;
            }
        } else if ((uint32_t)(now_ms - start_ms) >= PD_VBUS_ADC_TIMEOUT_MS) {
            /* Don't keep rearming a broken ADC. Never use a timed-out
             * sensor to decide whether power recovery is safe. */
            pending = false;
            timeouts++;
            failed = true;
            pd_hr_vbus_adc_failed(&hr_observer);
        }
        return;
    }
    /* Caution: compare by signed delta only when intervals <2^31 ms.
     * This is safe for the 10ms period, including the 32-bit wrap. */
    if ((int32_t)(now_ms - next_sample_ms) < 0) return;
    if (!adc_voltage_start_async()) {
        failed = true;
        timeouts++;
        pd_hr_vbus_adc_failed(&hr_observer);
        return;
    }
    pending = true;
    start_ms = now_ms;
    next_sample_ms = now_ms + PD_VBUS_SAMPLE_PERIOD_MS;
    pending_flags = (uint8_t)((cc_attached ? PD_VBUS_FLAG_ATTACHED : 0u) |
                      (ready ? PD_VBUS_FLAG_READY : 0u) |
                      (hard_reset ? PD_VBUS_FLAG_HARD_RESET : 0u));
}

void pd_vbus_monitor_stop(void) {
    active = false;
    /* Any in-flight conversion may complete after monitoring stops, but
     * must not be included in the frozen trace or initiate other work. */
    pending = false;
}

void pd_vbus_monitor_report_begin(void)
{
    report_cursor = 0u;
    report_started = false;
}

bool pd_vbus_monitor_report_step(void)
{
    /* The main loop checks !phy_attached() before calling us. Restrict
     * output to one line/iteration, matching the existing trace journal. */
    if (!report_started) {
        report_started = true;
        printf("PD17 VBUS BEGIN period_ms=%u count=%u total=%lu overwritten=%lu timeouts=%lu stopped_on_error=%u ready_n=%lu ready_min_raw=%u ready_max_raw=%u detach_seen=%u loss_detected=%u loss_raw=%u loss_rel_detach_ms=%ld\n",
               (unsigned)PD_VBUS_SAMPLE_PERIOD_MS,
               (unsigned)history.count, (unsigned long)history.total,
               (unsigned long)history.overwritten, (unsigned long)timeouts,
               (unsigned)failed, (unsigned long)history.ready_samples,
               (unsigned)(history.ready_samples ? history.min_ready_raw : 0u),
               (unsigned)history.max_ready_raw, (unsigned)detached_seen,
               (unsigned)loss_detected, (unsigned)loss_raw,
               (long)(loss_detected ? (int32_t)(loss_time_ms - detach_ms) : 0));
        /* Additional detached-only observation. Values are ADC indications,
           not certification of USB PD vSafe transitions. */
        printf("PD17 HR VBUS state=%u error=%u low_seen=%u low_raw=%u high_seen=%u high_raw=%u low_rel_hr_ms=%lu high_rel_hr_ms=%lu recovery_handoff=%u auto_recovery=%u pps_recovery=%u\n",
               (unsigned)hr_observer.state, (unsigned)hr_observer.error,
               (unsigned)hr_observer.low_seen, (unsigned)hr_observer.low_raw,
               (unsigned)hr_observer.high_seen, (unsigned)hr_observer.high_raw,
               (unsigned long)(hr_observer.low_seen ?
                   (hr_observer.low_time_ms - hr_observer.started_ms) : 0u),
               (unsigned long)(hr_observer.high_seen ?
                   (hr_observer.high_time_ms - hr_observer.started_ms) : 0u),
               (unsigned)hr_handoff, (unsigned)hr_autorecovered,
               (unsigned)hr_pps_recovered);
        return false;
    }
    if (report_cursor < history.count) {
        pd_vbus_point_t p;
        if (pd_vbus_history_get(&history, report_cursor++, &p)) {
            /* Offset to the CONFIRMED CC detach (150ms debounce), not the
             * physical instant the connector was unplugged. */
            int32_t relative_ms = (int32_t)(p.time_ms - detach_ms);
            printf("PD17 VBUS rel_detach_ms=%ld raw=%u uncal_mv=%lu flags=%u\n",
                   (long)relative_ms, (unsigned)p.adc_raw,
                   (unsigned long)adc_voltage_vbus_mv(p.adc_raw),
                   (unsigned)p.flags);
        }
        return false;
    }
    printf("PD17 VBUS END\n");
    return true;
}
