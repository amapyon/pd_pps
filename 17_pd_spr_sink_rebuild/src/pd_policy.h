#ifndef PD_POLICY_H
#define PD_POLICY_H
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PE_DETACHED, PE_WAIT_CAPS, PE_WAIT_ACCEPT, PE_WAIT_PS_RDY,
    PE_READY, PE_SOFT_RESET_WAIT, PE_RECOVERY
} pe_state_t;
typedef enum {
    PE_EV_ATTACH, PE_EV_DETACH, PE_EV_CAPS, PE_EV_ACCEPT,
    PE_EV_PS_RDY, PE_EV_REJECT, PE_EV_WAIT, PE_EV_SOFT_RESET_RX,
    PE_EV_HARD_RESET_RX, PE_EV_TX_FAILED, PE_EV_VBUS_LOST,
    PE_EV_HARD_RESET_TX, PE_EV_HR_VBUS_READY, PE_EV_SENSOR_FAULT
} pe_event_t;
typedef enum { PE_CMD_NONE, PE_CMD_REQUEST, PE_CMD_SOFT_RESET, PE_CMD_ACCEPT_SOFT_RESET, PE_CMD_GET_SOURCE_CAP } pe_cmd_t;
typedef struct { uint32_t pdo[7]; uint8_t count; uint8_t msg_id; } pe_caps_t;
typedef struct {
    pe_state_t state;
    bool contract_valid, pps_active, pending_pps, request_sent;
    bool caps_fresh, pending_soft_reset, awaiting_hr_recovery;
    uint8_t pdo_index, caps_epoch;
    uint32_t last_pps_ms, deadline_ms, defer_until_ms;
    uint32_t requested_mv, requested_ma;
    pe_caps_t caps;
    pe_cmd_t command;
    uint32_t rdo;
    uint8_t object_pos;
    uint16_t faults;
} pe_t;

void pe_init(pe_t *pe);
void pe_event(pe_t *pe, pe_event_t ev, const pe_caps_t *caps, uint32_t now_ms);
void pe_tick(pe_t *pe, uint32_t now_ms);
bool pe_request_soft_reset(pe_t *pe, uint32_t now_ms);
/* Bench-only: initiate Get_Source_Cap while READY; explicit contract unchanged. */
bool pe_request_source_caps(pe_t *pe);
void pe_command_result(pe_t *pe, bool accepted, uint32_t now_ms);
#endif
