#include "pd_9v_policy.h"
#include <assert.h>
#include <stdio.h>

#define FIXED_PDO(mv, ma) ((((uint32_t)(mv) / 50u) << 10) | ((ma) / 10u))

int main(void)
{
    pd_9v_request_t req = {0};
    /* Realistic fixed 5V/3A, 9V/2.22A, 12V/1.67A profiles */
    const uint32_t source_caps[] = {
        FIXED_PDO(5000, 3000), FIXED_PDO(9000, 2220),
        FIXED_PDO(12000, 1670), 0xc0000000u /* APDO, not fixed */
    };
    assert(pd_9v_select_request(source_caps, 4, &req));
    assert(req.object_position == 2);
    assert(req.voltage_mv == 9000);
    assert(req.source_current_ma == 2220);
    assert(req.requested_current_ma == 500);
    assert(req.request_rdo == (2u << 28 | 1u << 24 | 50u << 10 | 50u));

    const uint32_t no_9v[] = { FIXED_PDO(5000, 3000), FIXED_PDO(12000, 1670) };
    assert(!pd_9v_select_request(no_9v, 2, &req));
    const uint32_t underpowered[] = { FIXED_PDO(9000, 400) };
    assert(!pd_9v_select_request(underpowered, 1, &req));
    assert(!pd_9v_select_request(NULL, 1, &req));
    assert(!pd_9v_select_request(source_caps, 0, &req));
    puts("PASS: 9 V Fixed PDO and 500 mA RDO selection");
    return 0;
}
