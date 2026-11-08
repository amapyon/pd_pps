#ifndef PD_HR_BENCH_H
#define PD_HR_BENCH_H
#include <stdbool.h>
#include <stdint.h>
/* Bench-only single-shot eligibility. No hardware access or USB commands. */
#define PD_HR_BENCH_SETTLE_MS 500u
typedef struct {
    bool saw_renegotiation;
    bool stable_ready_seen;
    bool attempted;
    uint32_t ready_since_ms;
} pd_hr_bench_t;
void pd_hr_bench_init(pd_hr_bench_t *b);
bool pd_hr_bench_should_send(pd_hr_bench_t *b, uint32_t now_ms,
                             bool get_caps_once, bool ready_contract,
                             bool phy_idle_and_adc_idle);
#endif
