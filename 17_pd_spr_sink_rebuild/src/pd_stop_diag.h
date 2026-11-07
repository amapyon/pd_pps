#ifndef PD_STOP_DIAG_H
#define PD_STOP_DIAG_H
#include <stdbool.h>
#include <stdint.h>
#include "pd_policy.h"

/* M13c-2: ONE first-stop snapshot per physical CC attachment, not a
 * USB PD compliance verdict. All calls are foreground-only, no I/O/IRQ.
 * Reason 1 (VBUS loss) is also expected on deliberate cable removal. */
typedef enum {
    PD_STOP_NONE = 0,
    PD_STOP_VBUS_LOSS = 1,
    PD_STOP_ADC_MONITOR = 2,
    PD_STOP_PE_CAPS = 3,
    PD_STOP_PE_TX_FAILED = 4,
    PD_STOP_PE_TIMEOUT_CAPS = 5,
    PD_STOP_PE_TIMEOUT_ACCEPT = 6,
    PD_STOP_PE_TIMEOUT_PS_RDY = 7,
    PD_STOP_PE_TIMEOUT_SOFT_RESET = 8,
    PD_STOP_PE_OTHER = 9,
    PD_STOP_HR_SECOND_RX = 10,
    PD_STOP_HR_VBUS = 11,
    PD_STOP_HR_RX_ARM_TIMEOUT = 12,
    PD_STOP_HR_COMMIT_TIMEOUT = 13,
    PD_STOP_HR_BAD_PE_STATE = 14
} pd_stop_reason_t;

typedef struct {
    bool active, found, detach_seen;
    uint32_t started_ms, stop_rel_ms, detach_after_stop_ms;
    uint32_t hr_rx_count;
    uint16_t aux;
    uint8_t reason, pe_before, pe_after;
} pd_stop_diag_t;

void pd_stop_diag_init(pd_stop_diag_t *d);
void pd_stop_diag_attach(pd_stop_diag_t *d, uint32_t now_ms);
void pd_stop_diag_rx_hr(pd_stop_diag_t *d);
void pd_stop_diag_note(pd_stop_diag_t *d, pd_stop_reason_t why,
                       uint32_t now_ms, pe_state_t before,
                       pe_state_t after, uint16_t aux);
void pd_stop_diag_detach(pd_stop_diag_t *d, uint32_t now_ms);
pd_stop_reason_t pd_stop_diag_pe_tick_reason(pe_state_t before);
pd_stop_reason_t pd_stop_diag_pe_event_reason(pe_event_t event);
#endif
