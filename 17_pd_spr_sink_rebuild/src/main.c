#include <stdio.h>
/* M13b normal-mode SPR PPS sink prototype. INA226 is passive telemetry. */
#include "ch32fun.h"
#include "pd_policy.h"
#include "pd_phy_ch32x035.h"
#include "pd_sensors.h"
#include "pd_vbus_monitor.h"
#include "adc_voltage.h"
#include "pd_hr_recovery.h"
#include "pd_stability.h"
/* NORMAL MODE: no deliberate Hard Reset, no periodic Get_Source_Cap
 * stimulus, and no post-HR APDO re-probe. The Sink accepts the capabilities
 * actually offered by the Source on each negotiation/recovery. */
static pe_t policy;
static uint32_t now_ms(void) {
    uint32_t hi1, hi2, lo;
    do { hi1=SysTick->CNTH; lo=SysTick->CNTL; hi2=SysTick->CNTH; } while (hi1 != hi2);
    uint64_t cycles = ((uint64_t)hi1 << 32) | lo;
    return (uint32_t)(cycles / (FUNCONF_SYSTEM_CORE_CLOCK / 1000u));
}
int main(void) {
    SystemInit();
    SysTick->CTLR = SYSTICK_CTLR_STE | SYSTICK_CTLR_STCLK;
    pe_init(&policy);
    phy_init();
    pd_sensors_init(); /* passive sensing; no PD AMS calls */
    pd_vbus_monitor_init(); /* M13a-2: nonblocking 10ms ADC sampling */
    uint32_t last_heartbeat_ms = 0;
    uint32_t detach_time_ms = 0;
    bool trace_dump_pending = false;
    bool trace_dump_started = false;
    bool trace_dump_finished = false;
    bool sensor_detach_pending = false;
    bool sensor_ready_seen = false;
    bool sensor_ready_sampled = false;
    uint32_t sensor_first_ready_ms = 0;
    pd_hr_recovery_t m13b_recovery;
    pd_hr_recovery_init(&m13b_recovery);
    pd_stability_t m13c_stats;
    pd_stability_init(&m13c_stats);
    for (;;) {
        uint32_t now=now_ms();
        phy_poll_cc(now);
        /* Complete any in-flight ADC conversion BEFORE serving PE or TX
           retry. This makes the sustained loss stop effective as early as
           the foreground can act; no ADC work occurs in the PD ISR. */
        pd_vbus_monitor_tick(now, phy_attached(),
                             policy.state == PE_READY && policy.contract_valid,
                             phy_hard_reset_seen());
        if (pd_vbus_monitor_loss_detected() && phy_attached() &&
            !phy_hard_reset_seen() && !phy_vbus_loss_latched() &&
            policy.contract_valid) {
            uint16_t raw = pd_vbus_monitor_loss_raw();
            pe_state_t before = policy.state;
            /* Abort rechecks Hard Reset after masking USBPD_IRQn. A real
               Hard Reset wins even if it raced with this ADC decision. */
            if (phy_abort_for_vbus_loss(raw, adc_voltage_vbus_mv(raw))) {
                pe_event(&policy, PE_EV_VBUS_LOST, NULL, now);
                pd_stability_event(&m13c_stats, PE_EV_VBUS_LOST, before, &policy);
                phy_trace_note(PDTR_PE_EVENT, PE_EV_VBUS_LOST,
                               (uint16_t)before, (uint32_t)policy.state);
            }
        }
        /* A broken ADC cannot certify that a live explicit contract
           remains powered. Stop all PD TX/GoodCRC and invalidate the PE
           contract immediately; do not infer that VBUS actually fell. */
        if (pd_vbus_monitor_stopped_on_error() && phy_attached() &&
            policy.state != PE_RECOVERY && policy.state != PE_DETACHED &&
            !phy_hard_reset_seen() && !phy_vbus_loss_latched()) {
            pe_state_t before = policy.state;
            if (phy_abort_for_adc_failure()) {
                pe_event(&policy, PE_EV_SENSOR_FAULT, NULL, now);
                pd_stability_event(&m13c_stats, PE_EV_SENSOR_FAULT,
                                   before, &policy);
                phy_trace_note(PDTR_PE_EVENT, PE_EV_SENSOR_FAULT,
                               (uint16_t)before, (uint32_t)policy.state);
            }
        }
        phy_maintain(now);
        pe_event_t ev;
        pe_caps_t caps;
        for (uint8_t i=0; i<4 && phy_pop(&ev, &caps); i++) {
            pe_state_t before = policy.state;
            if (ev == PE_EV_DETACH)
                pd_stability_detach(&m13c_stats, now,
                                    phy_trace_dropped(), phy_lost_events(),
                                    policy.faults);
            pe_event(&policy,ev,ev==PE_EV_CAPS?&caps:0,now);
            if (ev != PE_EV_DETACH) pd_stability_event(&m13c_stats, ev, before, &policy);
            phy_trace_note(PDTR_PE_EVENT, (uint8_t)ev,
                           (uint16_t)before, (uint32_t)policy.state);
            if (ev==PE_EV_ATTACH) {
                /* New physical CC session: allow at most one received Hard
                   Reset recovery on this cable attachment. */
                pd_hr_recovery_new_session(&m13b_recovery);
                pd_stability_attach(&m13c_stats, now,
                                    phy_trace_dropped(), phy_lost_events());
                pd_vbus_monitor_attach(now);
                sensor_detach_pending = false;
            }
            if (ev==PE_EV_HARD_RESET_RX) {
                /* Source-originated Hard Reset: invalidate the contract,
                   observe vSafe0V -> vSafe5V conservatively, then restart
                   the protocol once; a second HR on the same attachment
                   remains fail-closed. Physical detach resets the quota. */
                if (pd_hr_recovery_begin(&m13b_recovery)) {
                    pd_vbus_monitor_hr_begin(now);
                } else {
                    /* A second Hard Reset is a terminal bench failure.
                       Do not create an unbounded Hard Reset loop. */
                    phy_hr_fail_stop();
                }
            }
            if (ev==PE_EV_DETACH) {
                pd_hr_recovery_detached(&m13b_recovery);
                pd_vbus_monitor_detach(now);
                /* The detached one-shot may not overlap an in-flight ADC
                   conversion. Capture it on the first idle iteration. */
                sensor_detach_pending = true;
                phy_reset_after_detach();
                trace_dump_pending = true;
                trace_dump_started = false;
                trace_dump_finished = false;
                detach_time_ms = now;
            }
        }
        /* The transition to default is handled in two phases: RX is armed
           after the first valid 5V sample, but PE/PHY restart is forbidden
           until the independent three-sample validation is complete. */
        pd_hr_recovery_action_t hr_action = pd_hr_recovery_step(
            &m13b_recovery, pd_vbus_monitor_hr_observation(),
            phy_attached(), phy_hard_reset_seen(),
            phy_hard_reset_tx_timed_out());
        if (hr_action == PD_HR_REC_ARM_RX) {
            if (phy_hr_arm_rx()) pd_hr_recovery_armed(&m13b_recovery);
        } else if (hr_action == PD_HR_REC_COMMIT) {
            if (policy.state == PE_RECOVERY &&
                phy_hr_complete_recovery()) {
                pe_state_t before = policy.state;
                pe_event(&policy, PE_EV_HR_VBUS_READY, NULL, now);
                phy_trace_note(PDTR_PE_EVENT, PE_EV_HR_VBUS_READY,
                               (uint16_t)before, (uint32_t)policy.state);
                if (policy.state == PE_WAIT_CAPS) {
                    pd_hr_recovery_completed(&m13b_recovery);
                    pd_vbus_monitor_hr_handoff();
                } else {
                    phy_hr_fail_stop();
                    pd_hr_recovery_failed(&m13b_recovery);
                }
            }
        } else if (hr_action == PD_HR_REC_STOP) {
            phy_hr_fail_stop();
            pd_hr_recovery_failed(&m13b_recovery);
        }
        pe_state_t state_before_tick = policy.state;
        bool was_pps_ready = state_before_tick == PE_READY &&
                             policy.contract_valid && policy.pps_active;
        pe_tick(&policy,now);
        if (was_pps_ready && policy.state == PE_WAIT_ACCEPT)
            pd_stability_refresh_started(&m13c_stats);
        if (state_before_tick != PE_RECOVERY && policy.state == PE_RECOVERY)
            pd_stability_event(&m13c_stats, PE_EV_TX_FAILED,
                               state_before_tick, &policy);
        /* A generic PE timeout (even without HR) must stop PHY acknowledgments
           and retransmissions. Only a fresh physical CC attach may clear it. */
        if (phy_attached() && policy.state == PE_RECOVERY &&
            !phy_hard_reset_seen() && !phy_vbus_loss_latched())
            phy_abort_for_policy_failure((uint16_t)state_before_tick);
        /* A timeout after protocol-layer restart must not leave the PHY
           acknowledging SOP messages forever in a PE_RECOVERY state. */
        if (m13b_recovery.completed && policy.state == PE_RECOVERY) {
            phy_hr_fail_stop();
            pd_hr_recovery_failed(&m13b_recovery);
        }
        /* An explicit Ready after the HR handoff proves PD recovery, even
           when the Source now offers only fixed 5V. Log PPS separately. */
        if (m13b_recovery.completed && policy.state == PE_READY &&
            policy.contract_valid)
            pd_vbus_monitor_hr_contract_ready(policy.pps_active);
        if (policy.state != state_before_tick)
            phy_trace_note(PDTR_PE_TICK, (uint8_t)state_before_tick,
                           (uint16_t)policy.state, (uint32_t)policy.faults);
        /* Passive one-shot measurement while stable in READY, 300ms after
           first READY. Never measure during Request/Accept/PS_RDY exchange.
           Store results; do not print until physical CC detachment. */
        if (!sensor_ready_seen && phy_attached() && policy.state == PE_READY &&
            policy.contract_valid) {
            sensor_ready_seen = true;
            sensor_first_ready_ms = now;
        }
        if (pd_sensors_ready_due(sensor_ready_sampled, phy_attached(),
                                 policy.state == PE_READY && policy.contract_valid &&
                                 policy.command == PE_CMD_NONE,
                                 now, sensor_first_ready_ms) && sensor_ready_seen) {
            /* An incoming Hard Reset is latched by PHY; never sample while
               reset is active. It is only passive telemetry in any case. */
            if (!phy_hard_reset_seen() && !pd_vbus_monitor_busy()) {
                sensor_ready_sampled = true;
                pd_sensors_capture_ready();
            }
        }
        if (policy.command != PE_CMD_NONE) {
            pe_cmd_t cmd=policy.command;
            bool ok=phy_send(cmd,policy.rdo,now);
            if (ok) pd_stability_command_sent(&m13c_stats, cmd);
            pe_command_result(&policy,ok,now);
        }
        pd_stability_tick(&m13c_stats, now,
                          policy.state == PE_READY && policy.contract_valid);
        if (sensor_detach_pending && !pd_vbus_monitor_busy()) {
            pd_sensors_capture_detached();
            sensor_detach_pending = false;
        }
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
                               (unsigned)policy.state, (unsigned)policy.faults);
                        /* Re-read trace overflow after detached-only PE
                           journal entries have been recorded. */
                        m13c_stats.trace_drops_delta =
                            phy_trace_dropped() - m13c_stats.trace_drops_at_start;
                        /* Re-read trace overflow after detached-only PE
                           journal entries have been recorded. */
                        m13c_stats.trace_drops_delta =
                            phy_trace_dropped() - m13c_stats.trace_drops_at_start;
                        printf("PD17 STABILITY session_ms=%lu ready_ms=%lu loop_gap_max_ms=%lu caps=%lu requests=%lu pps_ready=%lu fixed_ready=%lu refresh_started=%lu refresh_done=%lu\n",
                               (unsigned long)m13c_stats.elapsed_ms,
                               (unsigned long)m13c_stats.ready_ms,
                               (unsigned long)m13c_stats.longest_loop_ms,
                               (unsigned long)m13c_stats.caps_received,
                               (unsigned long)m13c_stats.requests_issued,
                               (unsigned long)m13c_stats.pps_ready,
                               (unsigned long)m13c_stats.fixed_ready,
                               (unsigned long)m13c_stats.refresh_started,
                               (unsigned long)m13c_stats.refresh_completed);
                        printf("PD17 STABILITY ERR source_hr=%lu pe_recovery=%lu vbus_loss=%lu adc_fault=%lu policy_faults=%lu trace_drop_session=%lu event_lost_session=%lu\n",
                               (unsigned long)m13c_stats.source_hr_rx,
                               (unsigned long)m13c_stats.recovery_entries,
                               (unsigned long)m13c_stats.vbus_loss,
                               (unsigned long)m13c_stats.adc_fault,
                               (unsigned long)m13c_stats.faults_at_end,
                               (unsigned long)m13c_stats.trace_drops_delta,
                               (unsigned long)m13c_stats.lost_delta);
                        pd_sensors_report_detached();
                        printf("PD17 MODE normal=1 bench_hr_tx=0 bench_get_caps=0 pps_reprobe=0 hr_rx_max_per_attach=1 m13c_stats=1 adc_fail_stop=1\n");
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
                pd_sensors_clear();
                pd_vbus_monitor_clear();
                sensor_ready_seen = false;
                sensor_ready_sampled = false;
                trace_dump_pending = false;
            }
        }
        /* Detached-only heartbeat from M8; suppressed while draining trace. */
        if (!phy_attached() && !trace_dump_pending &&
            (uint32_t)(now - last_heartbeat_ms) >= 2000u) {
            last_heartbeat_ms = now;
            printf("PD17: alive, charger detached, state=%u, lost=%lu\n",
                   (unsigned)policy.state, (unsigned long)phy_lost_events());
        }
        /* No synchronous printf/OLED/I2C while a PD connection is active. */
    }
}
