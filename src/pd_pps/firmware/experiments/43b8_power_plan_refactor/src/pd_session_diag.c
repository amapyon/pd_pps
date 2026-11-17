#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "pd_session_diag.h"
#include "pd_phy_ch32x035.h"
#include "pd_vbus_monitor.h"
#include "pd_sensors.h"
#include "m43b2_diag.h"

/* Output-only phase. Never called while the PD cable is attached. */
static uint32_t last_heartbeat_ms;
static uint32_t detach_time_ms;
static bool trace_dump_pending;
static bool trace_dump_started;
static bool trace_dump_finished;

void pd_diag_init(void) {
    last_heartbeat_ms = 0;
    detach_time_ms = 0;
    trace_dump_pending = false;
    trace_dump_started = false;
    trace_dump_finished = false;
}

void pd_diag_on_detach(uint32_t now) {
    trace_dump_pending = true;
    trace_dump_started = false;
    trace_dump_finished = false;
    detach_time_ms = now;
}

/* True only when the last VBUS report line has been emitted. The caller
 * then performs the original sensor/monitor cleanup before clearing pending. */
bool pd_diag_report_step(uint32_t now, pe_t *policy,
                         pd_stability_t *stats, pd_stop_diag_t *stop) {
        /* Drain recorded events only after confirmed CC detach + 500 ms.
           Keep the charger unplugged through PD17 VBUS END; printing blocks.
           The VBUS trace is frozen BEFORE any printing starts. */
        if (trace_dump_pending && !phy_attached() &&
            (uint32_t)(now - detach_time_ms) >= 500u) {
            if (!trace_dump_started) {
                pd_vbus_monitor_stop();
                trace_dump_started = true;
                printf("PD17 TRACE BEGIN (tick=SysTick CNTL, low 32 bits)\n");
            }
            if (!trace_dump_finished) {
                pd_trace_t t;
                for (uint8_t j=0; j<4u && !phy_attached(); j++) {
                    if (!phy_trace_pop(&t)) {
                        printf("PD17 TRACE END trace_dropped=%lu event_lost=%lu state=%u faults=%u\n",
                               (unsigned long)phy_trace_dropped(),
                               (unsigned long)phy_lost_events(),
                               (unsigned)policy->state, (unsigned)policy->faults);
                        /* Re-read trace overflow after detached-only PE
                           journal entries have been recorded. */
                        (*stats).trace_drops_delta =
                            phy_trace_dropped() - (*stats).trace_drops_at_start;
                        printf("PD17 STABILITY session_ms=%lu ready_ms=%lu loop_gap_max_ms=%lu caps=%lu requests=%lu pps_ready=%lu fixed_ready=%lu refresh_started=%lu refresh_done=%lu\n",
                               (unsigned long)(*stats).elapsed_ms,
                               (unsigned long)(*stats).ready_ms,
                               (unsigned long)(*stats).longest_loop_ms,
                               (unsigned long)(*stats).caps_received,
                               (unsigned long)(*stats).requests_issued,
                               (unsigned long)(*stats).pps_ready,
                               (unsigned long)(*stats).fixed_ready,
                               (unsigned long)(*stats).refresh_started,
                               (unsigned long)(*stats).refresh_completed);
                        printf("PD17 STABILITY ERR source_hr=%lu pe_recovery=%lu vbus_loss=%lu adc_fault=%lu policy_faults=%lu trace_drop_session=%lu event_lost_session=%lu\n",
                               (unsigned long)(*stats).source_hr_rx,
                               (unsigned long)(*stats).recovery_entries,
                               (unsigned long)(*stats).vbus_loss,
                               (unsigned long)(*stats).adc_fault,
                               (unsigned long)(*stats).faults_at_end,
                               (unsigned long)(*stats).trace_drops_delta,
                               (unsigned long)(*stats).lost_delta);
                        m43b2_diag_t m43b2;
                        m43b2_diag_snapshot(&m43b2);
                        printf("PD17 M43B2 IRQ pd=%lu fs=%lu fs_crc=%lu pd_nested_fs=%lu fs_max_cycles=%lu status_or=%04lx buf_err_samples=%lu\n",
                               (unsigned long)m43b2.pd_irq,
                               (unsigned long)m43b2.usbfs_irq,
                               (unsigned long)m43b2.usbfs_crc_window,
                               (unsigned long)m43b2.pd_preempted_usbfs,
                               (unsigned long)m43b2.usbfs_max_cycles,
                               (unsigned long)m43b2.pd_status_or,
                               (unsigned long)m43b2.buf_err_status_samples);
                        printf("PD17 M43B2 CRC tx_end=%lu rx_act=%lu arm_n=%lu arm_max_cycles=%lu arm_last_cycles=%lu goodcrc=%lu match=%lu mismatch=%lu crc_window_n=%lu crc_max_cycles=%lu retry=%lu tx_failed=%lu\n",
                               (unsigned long)m43b2.tx_end,
                               (unsigned long)m43b2.rx_act,
                               (unsigned long)m43b2.rx_arm_count,
                               (unsigned long)m43b2.rx_arm_max_cycles,
                               (unsigned long)m43b2.rx_arm_last_cycles,
                               (unsigned long)m43b2.goodcrc_seen,
                               (unsigned long)m43b2.goodcrc_match,
                               (unsigned long)m43b2.goodcrc_mismatch,
                               (unsigned long)m43b2.goodcrc_window_samples,
                               (unsigned long)m43b2.goodcrc_max_cycles,
                               (unsigned long)m43b2.retry,
                               (unsigned long)m43b2.tx_failed);
                        printf("PD17 STOP reason=%u first_rel_attach_ms=%lu pe_before=%u pe_after=%u aux=%u hr_rx=%lu detach_seen=%u detach_after_stop_ms=%lu\n",
                               (unsigned)(*stop).reason,
                               (unsigned long)(*stop).stop_rel_ms,
                               (unsigned)(*stop).pe_before,
                               (unsigned)(*stop).pe_after,
                               (unsigned)(*stop).aux,
                               (unsigned long)(*stop).hr_rx_count,
                               (unsigned)(*stop).detach_seen,
                               (unsigned long)(*stop).detach_after_stop_ms);
                        pd_sensors_report_detached();
                        printf("PD17 MODE normal=1 bench_hr_tx=0 bench_get_caps=0 pps_reprobe=0 hr_rx_max_per_attach=1 m13c_stats=1 adc_fail_stop=1 m13c2_stop_diag=1 hr_gate_budget_ms=20\n");
                        pd_vbus_monitor_report_begin();
                        trace_dump_finished = true;
                        break;
                    }
                    printf("PD17 TRACE tick=%08lx code=%u detail=%u aux=%04x value=%08lx\n",
                           (unsigned long)t.tick, (unsigned)t.kind,
                           (unsigned)t.detail, (unsigned)t.aux,
                           (unsigned long)t.value);
                }
            } else if (pd_vbus_monitor_report_step()) {
                return true;
            }
        }
    return false;
}

void pd_diag_finish_cleanup(void) {
    trace_dump_pending = false;
}

void pd_diag_heartbeat(uint32_t now, const pe_t *policy) {
        /* Detached-only heartbeat from M8; suppressed while draining trace. */
        if (!phy_attached() && !trace_dump_pending &&
            (uint32_t)(now - last_heartbeat_ms) >= 2000u) {
            last_heartbeat_ms = now;
            printf("PD17: alive, charger detached, state=%u, lost=%lu\n",
                   (unsigned)policy->state, (unsigned long)phy_lost_events());
        }
}
