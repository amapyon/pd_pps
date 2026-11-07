#include <assert.h>
#include <stdio.h>
#include "pd_vbus_history.h"

int main(void)
{
    pd_vbus_history_t h = {0};
    pd_vbus_point_t p;
    pd_vbus_history_clear(&h);
    assert(h.count == 0 && h.min_ready_raw == 0xffffu);
    assert(!pd_vbus_history_get(&h, 0, &p));
    pd_vbus_history_append(&h, 1, 790, PD_VBUS_FLAG_ATTACHED | PD_VBUS_FLAG_READY);
    pd_vbus_history_append(&h, 2, 789, PD_VBUS_FLAG_ATTACHED | PD_VBUS_FLAG_READY);
    pd_vbus_history_append(&h, 3, 15, 0);
    assert(h.ready_samples == 2 && h.min_ready_raw == 789 && h.max_ready_raw == 790);
    assert(pd_vbus_history_get(&h, 0, &p) && p.time_ms == 1 && p.adc_raw == 790);
    assert(pd_vbus_history_get(&h, 2, &p) && p.adc_raw == 15 && p.flags == 0);
    for (uint32_t i = 0; i < 250; i++) {
        pd_vbus_history_append(&h, i + 100, (uint16_t)i,
                               i & 1 ? PD_VBUS_FLAG_ATTACHED : 0u);
    }
    assert(h.count == PD_VBUS_HISTORY_LEN);
    assert(h.total == 253 && h.overwritten == 253 - PD_VBUS_HISTORY_LEN);
    assert(pd_vbus_history_get(&h, 0, &p) && p.time_ms == 100 + 122);
    assert(pd_vbus_history_get(&h, 127, &p) && p.time_ms == 349);
    assert(!pd_vbus_history_get(&h, 128, &p));
    pd_vbus_history_clear(&h);
    assert(h.count == 0 && h.overwritten == 0 && h.ready_samples == 0);
    puts("M13a-2 fixed-size VBUS ring: PASS");
    return 0;
}
