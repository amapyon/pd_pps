#include "pd_hr_bench.h"
void pd_hr_bench_init(pd_hr_bench_t *b) { *b=(pd_hr_bench_t){0}; }
bool pd_hr_bench_should_send(pd_hr_bench_t *b, uint32_t now_ms,
                             bool get_caps_once, bool ready_contract,
                             bool phy_idle_and_adc_idle) {
    if (b->attempted) return false;
    /* The first READY with Get_Source_Cap pending is insufficient. We must
       observe a transition out of READY and back into READY after fresh
       Source_Capabilities were processed by the actual PE. */
    if (get_caps_once && !ready_contract) b->saw_renegotiation = true;
    if (!b->saw_renegotiation) return false;
    if (!ready_contract) { b->stable_ready_seen = false; return false; }
    if (!b->stable_ready_seen) {
        b->stable_ready_seen = true;
        b->ready_since_ms = now_ms;
        return false;
    }
    if ((uint32_t)(now_ms - b->ready_since_ms) < PD_HR_BENCH_SETTLE_MS ||
        !phy_idle_and_adc_idle) return false;
    b->attempted = true;
    return true;
}
