#ifndef PD_PHY_CH32X035_H
#define PD_PHY_CH32X035_H
#include <stdbool.h>
#include <stdint.h>
#include "pd_policy.h"
/* M9: fixed-size, RAM-only trace. Never print from IRQ / while attached.
   tick is the low 32 bits of SysTick cycles (wraps about every 89 s at 48 MHz). */
typedef struct {
    uint32_t tick;
    uint32_t value;
    uint16_t aux;
    uint8_t kind;
    uint8_t detail;
} pd_trace_t;

enum {
    PDTR_CC_ATTACH = 1, PDTR_CC_DETACH,
    PDTR_IRQ_RESET, PDTR_IRQ_TX_END, PDTR_IRQ_NON_SOP,
    PDTR_RX_BAD_LENGTH, PDTR_RX_HEADER, PDTR_RX_PDO,
    PDTR_TX_GOODCRC, PDTR_RX_GOODCRC, PDTR_RX_DUPLICATE,
    PDTR_TX_COMMAND, PDTR_TX_RETRY, PDTR_TX_FAILED,
    PDTR_PE_EVENT, PDTR_PE_TICK, PDTR_TX_DISCARDED, PDTR_VBUS_ABORT,
    PDTR_TX_HARD_RESET, PDTR_TX_HARD_RESET_END, PDTR_TX_HARD_RESET_TIMEOUT,
    PDTR_HR_RX_ARM, PDTR_HR_RECOVERY, PDTR_HR_FAIL_STOP, PDTR_HR_EARLY_CAPS, PDTR_HR_PPS_REPROBE,
    PDTR_ADC_FAIL_STOP, PDTR_PE_FAIL_STOP
};
void phy_trace_note(uint8_t kind, uint8_t detail, uint16_t aux, uint32_t value);
bool phy_trace_pop(pd_trace_t *out);
uint32_t phy_trace_dropped(void);
void phy_init(void);
void phy_poll_cc(uint32_t now_ms);
void phy_maintain(uint32_t now_ms);
void phy_reset_after_detach(void);
bool phy_pop(pe_event_t *type, pe_caps_t *caps);
bool phy_send(pe_cmd_t command, uint32_t rdo, uint32_t now_ms);
/* Bench-only one-shot Hard Reset signaling. Never resumes a PD contract. */
bool phy_send_hard_reset_once(void);
bool phy_hard_reset_tx_finished(void);
bool phy_hard_reset_tx_timed_out(void);
/* Two-stage restart: early receive acknowledges and stages SourceCaps, but
   only the post-ADC commit permits PE to evaluate the staged message. */
bool phy_hr_arm_rx(void);
bool phy_hr_complete_recovery(void);
void phy_hr_fail_stop(void);
/* M13c failure gates: both latch the PHY until a physical CC detach. */
bool phy_abort_for_adc_failure(void);
bool phy_abort_for_policy_failure(uint16_t pe_state);
bool phy_attached(void);
bool phy_hard_reset_seen(void);
bool phy_abort_for_vbus_loss(uint16_t adc_raw, uint32_t uncal_mv);
bool phy_vbus_loss_latched(void);
uint32_t phy_lost_events(void);
#endif
