#include "m4_apply_guard.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    m4_apply_check_t q = {
        .pd_ready = true,
        .original_target_unchanged = true,
        .source_apdo_unchanged = true,
        .source_power_limited = false,
        .apdo = {5000u, 11000u, 1800u},
        .mv = 7100u,
        .ma = 1000u,
    };
    assert(m4_apply_validate(&q) == M4_APPLY_OK);
    q.pd_ready = false;
    assert(m4_apply_validate(&q) == M4_APPLY_PD_NOT_READY);
    q.pd_ready = true;
    q.original_target_unchanged = false;
    assert(m4_apply_validate(&q) == M4_APPLY_TARGET_CHANGED);
    q.original_target_unchanged = true;
    q.source_apdo_unchanged = false;
    assert(m4_apply_validate(&q) == M4_APPLY_SOURCE_CHANGED);
    q.source_apdo_unchanged = true;
    q.source_power_limited = true;
    assert(m4_apply_validate(&q) == M4_APPLY_POWER_LIMITED);
    q.source_power_limited = false;
    q.mv = 11100u;
    assert(m4_apply_validate(&q) == M4_APPLY_OUT_OF_RANGE);
    q.mv = 7100u;
    q.ma = 1850u;
    assert(m4_apply_validate(&q) == M4_APPLY_OUT_OF_RANGE);
    q.ma = 1051u;
    assert(m4_apply_validate(&q) == M4_APPLY_OUT_OF_RANGE);
    q.ma = 150u;
    assert(m4_apply_validate(&q) == M4_APPLY_LOW_REQUEST);
    q.ma = 200u;
    assert(m4_apply_validate(&q) == M4_APPLY_OK);
    q.mv = 7110u;
    assert(m4_apply_validate(&q) == M4_APPLY_OUT_OF_RANGE);
    q.mv = 7100u;
    assert(m4_apply_validate(&q) == M4_APPLY_OK);
    assert(m4_apply_validate(0) == M4_APPLY_PD_NOT_READY);
    puts("PASS: live apply guard - ready, source identity, RDO steps/range, low-I, power-limited");
    return 0;
}
