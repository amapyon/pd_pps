#ifndef PD_9V_POLICY_H
#define PD_9V_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#define PD_9V_TARGET_MV       9000u
#define PD_9V_REQUEST_MA      500u
#define PD_9V_MAX_SOURCE_PDOS 7u

typedef struct {
    uint8_t object_position;      /* 1-based position from Source_Capabilities */
    uint16_t voltage_mv;          /* Always 9000 on success */
    uint16_t requested_current_ma;
    uint16_t source_current_ma;
    uint32_t request_rdo;         /* Fixed-Supply Request Data Object */
} pd_9v_request_t;

/*
 * Returns true only when an advertised 9 V Fixed-Supply PDO can
 * provide at least PD_9V_REQUEST_MA.
 * This routine is ONLY the policy/encoding layer. The caller must
 * run a real USB-PD protocol engine, transmit the resulting Request,
 * and await Accept + PS_RDY before treating VBUS as 9 V.
 */
bool pd_9v_select_request(const uint32_t *source_pdos,
                          uint8_t pdo_count,
                          pd_9v_request_t *result);

#endif
