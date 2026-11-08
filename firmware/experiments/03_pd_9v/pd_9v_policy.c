#include "pd_9v_policy.h"
#include <stddef.h>

#define PD_PDO_TYPE(pdo)          (((pdo) >> 30) & 0x3u)
#define PD_PDO_FIXED             0u
#define PD_PDO_VOLTAGE_MV(pdo)    ((((pdo) >> 10) & 0x3ffu) * 50u)
#define PD_PDO_CURRENT_MA(pdo)    (((pdo) & 0x3ffu) * 10u)

#define PD_RDO_OBJ_POSITION(pos)  (((uint32_t)(pos)) << 28)
#define PD_RDO_NO_USB_SUSPEND    (1u << 24)
#define PD_RDO_CURRENT_10MA(ma)  ((uint32_t)(ma) / 10u)

bool pd_9v_select_request(const uint32_t *source_pdos,
                          uint8_t pdo_count,
                          pd_9v_request_t *result)
{
    if (source_pdos == NULL || result == NULL ||
        pdo_count == 0 || pdo_count > PD_9V_MAX_SOURCE_PDOS)
        return false;

    for (uint8_t i = 0; i < pdo_count; ++i) {
        uint32_t pdo = source_pdos[i];
        if (PD_PDO_TYPE(pdo) != PD_PDO_FIXED)
            continue; /* PPS/APDO/variable/battery are not fixed PDOs */

        uint32_t mv = PD_PDO_VOLTAGE_MV(pdo);
        uint32_t ma = PD_PDO_CURRENT_MA(pdo);
        if (mv != PD_9V_TARGET_MV || ma < PD_9V_REQUEST_MA)
            continue;

        uint32_t units = PD_RDO_CURRENT_10MA(PD_9V_REQUEST_MA);
        result->object_position = (uint8_t)(i + 1);
        result->voltage_mv = PD_9V_TARGET_MV;
        result->requested_current_ma = PD_9V_REQUEST_MA;
        result->source_current_ma = (uint16_t)ma;
        result->request_rdo = PD_RDO_OBJ_POSITION(i + 1)
                            | PD_RDO_NO_USB_SUSPEND
                            | (units << 10) /* operating current */
                            | units;        /* max operating current */
        return true;
    }
    return false;
}
