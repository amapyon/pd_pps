#include <stdbool.h>
#include <stdint.h>
#include "ch32fun.h"
#include "pd_session.h"
#include "pd_session_diag.h"
#include "pd_policy.h"
#include "pd_phy_ch32x035.h"
#include "pd_sensors.h"
#include "pd_vbus_monitor.h"
#include "adc_voltage.h"
#include "pd_hr_recovery.h"
#include "pd_stability.h"
#include "pd_stop_diag.h"
#include "m38_usb.h"
#include "m42_diag.h"

/* Foreground session state: intentionally single-instance and not touched
 * from ISRs. PHY/USB interrupt implementations remain untouched. */
static pe_t policy;
static pd_hr_recovery_t m13b_recovery;
static pd_stability_t m13c_stats;
static pd_stop_diag_t m13c2_stop;
static bool sensor_detach_pending;
static bool sensor_ready_seen;
static bool sensor_ready_sampled;
static uint32_t sensor_first_ready_ms;

/* Called before the unchanged IRQ-priority and USB initialization sequence. */
void pd_session_init_core(void) {
    pe_init(&policy);
    phy_init();
    pd_sensors_init();
    pd_vbus_monitor_init();
    m42_diag_new_attach();
}

/* Called after GPIO LED initialization, as in M43B-6. */
void pd_session_init_state(void) {
    sensor_detach_pending = false;
    sensor_ready_seen = false;
    sensor_ready_sampled = false;
    sensor_first_ready_ms = 0;
    pd_hr_recovery_init(&m13b_recovery);
    pd_stability_init(&m13c_stats);
    pd_stop_diag_init(&m13c2_stop);
    pd_diag_init();
}

/* PHASE 1: Physical CC detection and synchronous safety stops before TX. */
static void session_safety_and_phy(uint32_t now) {
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
                pd_stop_diag_note(&m13c2_stop, PD_STOP_VBUS_LOSS,
                                  now, before, policy.state, raw);
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
                pd_stop_diag_note(&m13c2_stop, PD_STOP_ADC_MONITOR,
                                  now, before, policy.state,
                                  (uint16_t)pd_vbus_monitor_timeouts());
                pd_stability_event(&m13c_stats, PE_EV_SENSOR_FAULT,
                                   before, &policy);
                phy_trace_note(PDTR_PE_EVENT, PE_EV_SENSOR_FAULT,
                               (uint16_t)before, (uint32_t)policy.state);
            }
        }
        phy_maintain(now);
}

/* PHASE 2: Deliver up to four queued PHY events, preserving per-event order. */
static void session_receive_events(uint32_t now) {
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
            if (ev != PE_EV_DETACH && ev != PE_EV_HARD_RESET_RX &&
                before != PE_RECOVERY && policy.state == PE_RECOVERY)
                pd_stop_diag_note(&m13c2_stop,
                                  pd_stop_diag_pe_event_reason(ev),
                                  now, before, policy.state, (uint16_t)ev);
            phy_trace_note(PDTR_PE_EVENT, (uint8_t)ev,
                           (uint16_t)before, (uint32_t)policy.state);
            if (ev==PE_EV_ATTACH) {
                /* New physical CC session: allow at most one received Hard
                   Reset recovery on this cable attachment. */
                pd_hr_recovery_new_session(&m13b_recovery);
                pd_stop_diag_attach(&m13c2_stop, now);
                m42_diag_new_attach();
                pd_stability_attach(&m13c_stats, now,
                                    phy_trace_dropped(), phy_lost_events());
                pd_vbus_monitor_attach(now);
                sensor_detach_pending = false;
            }
            if (ev==PE_EV_HARD_RESET_RX) {
                sensor_ready_seen = false;
                sensor_ready_sampled = false;
                pd_sensors_reset_ready();
                pd_stop_diag_rx_hr(&m13c2_stop);
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
                    pd_stop_diag_note(&m13c2_stop, PD_STOP_HR_SECOND_RX,
                                      now, before, policy.state, 2u);
                }
            }
            if (ev==PE_EV_DETACH) {
                pd_stop_diag_detach(&m13c2_stop, now);
                pd_hr_recovery_detached(&m13b_recovery);
                pd_vbus_monitor_detach(now);
                /* The detached one-shot may not overlap an in-flight ADC
                   conversion. Capture it on the first idle iteration. */
                sensor_detach_pending = true;
                phy_reset_after_detach();
                pd_diag_on_detach(now);
            }
        }
}

/* PHASE 3: Hard-Reset recovery gate: no PD traffic during unsafe VBUS. */
static void session_hr_recovery(uint32_t now) {
        /* The transition to default is handled in two phases: RX is armed
           after the first valid 5V sample, but PE/PHY restart is forbidden
           until the independent three-sample validation is complete. */
        pd_hr_recovery_action_t hr_action = pd_hr_recovery_step(
            &m13b_recovery, pd_vbus_monitor_hr_observation(),
            phy_attached(), phy_hard_reset_seen(),
            phy_hard_reset_tx_timed_out());
        if (hr_action == PD_HR_REC_ARM_RX) {
            if (phy_hr_arm_rx()) {
                pd_hr_recovery_gate_ready(&m13b_recovery);
                pd_hr_recovery_armed(&m13b_recovery);
            } else if (pd_hr_recovery_gate_busy(&m13b_recovery,
                                                hr_action, now)) {
                phy_hr_fail_stop();
                pd_hr_recovery_failed(&m13b_recovery);
                pd_stop_diag_note(&m13c2_stop, PD_STOP_HR_RX_ARM_TIMEOUT,
                                  now, policy.state, policy.state, 0u);
            }
        } else if (hr_action == PD_HR_REC_COMMIT) {
            if (policy.state != PE_RECOVERY) {
                phy_hr_fail_stop();
                pd_hr_recovery_failed(&m13b_recovery);
                pd_stop_diag_note(&m13c2_stop, PD_STOP_HR_BAD_PE_STATE,
                                  now, policy.state, policy.state, 0u);
            } else if (phy_hr_complete_recovery()) {
                pd_hr_recovery_gate_ready(&m13b_recovery);
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
                    pd_stop_diag_note(&m13c2_stop, PD_STOP_HR_BAD_PE_STATE,
                                      now, before, policy.state, 1u);
                }
            } else if (pd_hr_recovery_gate_busy(&m13b_recovery,
                                                hr_action, now)) {
                phy_hr_fail_stop();
                pd_hr_recovery_failed(&m13b_recovery);
                pd_stop_diag_note(&m13c2_stop, PD_STOP_HR_COMMIT_TIMEOUT,
                                  now, policy.state, policy.state, 0u);
            }
        } else if (hr_action == PD_HR_REC_STOP) {
            phy_hr_fail_stop();
            pd_hr_recovery_failed(&m13b_recovery);
            const pd_hr_vbus_t *hr = pd_vbus_monitor_hr_observation();
            pd_stop_diag_note(&m13c2_stop, PD_STOP_HR_VBUS,
                              now, policy.state, policy.state,
                              (uint16_t)(((uint16_t)hr->state << 8) | hr->error));
        }
}

/* PHASE 4: Advance PE timers and fail-closed fallback. */
static void session_advance_policy(uint32_t now) {
        pe_state_t state_before_tick = policy.state;
        bool was_pps_ready = state_before_tick == PE_READY &&
                             policy.contract_valid && policy.pps_active;
        pe_tick(&policy,now);
        if (was_pps_ready && policy.state == PE_WAIT_ACCEPT) {
            pd_stability_refresh_started(&m13c_stats);
            phy_m43b5_mark_refresh(); /* M43B5: tag next accepted Request */
        }
        if (state_before_tick != PE_RECOVERY && policy.state == PE_RECOVERY) {
            pd_stability_event(&m13c_stats, PE_EV_TX_FAILED,
                               state_before_tick, &policy);
            pd_stop_diag_note(&m13c2_stop,
                              pd_stop_diag_pe_tick_reason(state_before_tick),
                              now, state_before_tick, policy.state, 0u);
        }
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
}

/* PHASE 5: Passive sensors, one TX command and stability accounting. */
static void session_sensors_and_tx(uint32_t now) {
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
}

/* PHASE 6: First-stop diagnostics and a single non-blocking CDC pass. */
static void session_cdc_after_pd(uint32_t now) {
        /* M42: freeze a bounded copy of PRE-EXISTING 10ms ADC samples when
           first-stop is recorded. Never starts ADC/I2C; no change to PD ISR. */
        m42_diag_capture_first_stop(&m13c2_stop);
        /* A single non-blocking CDC service pass, AFTER all PD safety and
         * negotiation work.  No USB recovery/reset while PD is attached. */
        m38_usb_service(now, &policy, phy_attached(),
                        &m13c_stats, &m13c2_stop);
}

/* Call order is a safety contract. Do not reorder these phases. */
void pd_session_step(uint32_t now) {
    session_safety_and_phy(now);
    session_receive_events(now);
    session_hr_recovery(now);
    session_advance_policy(now);
    session_sensors_and_tx(now);
    session_cdc_after_pd(now);
    if (pd_diag_report_step(now, &policy, &m13c_stats, &m13c2_stop)) {
        pd_sensors_clear();
        pd_vbus_monitor_clear();
        sensor_ready_seen = false;
        sensor_ready_sampled = false;
        pd_diag_finish_cleanup();
    }
    pd_diag_heartbeat(now, &policy);
}
