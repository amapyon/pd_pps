/* M13a-5: bench scheduler is real production code. No PD hardware. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "pd_hr_bench.h"

int main(void) {
    pd_hr_bench_t b;
    pd_hr_bench_init(&b);
    assert(!pd_hr_bench_should_send(&b, 0, false, true, true));
    assert(!pd_hr_bench_should_send(&b, 1000, true, true, true));
    assert(!pd_hr_bench_should_send(&b, 1100, true, false, false));
    assert(b.saw_renegotiation);
    assert(!pd_hr_bench_should_send(&b, 1200, true, true, true));
    assert(!pd_hr_bench_should_send(&b, 1699, true, true, true));
    /* ADC conversion active: does not reset settled-ready timer. */
    assert(!pd_hr_bench_should_send(&b, 1700, true, true, false));
    assert(pd_hr_bench_should_send(&b, 1701, true, true, true));
    assert(b.attempted);
    assert(!pd_hr_bench_should_send(&b, 9000, true, true, true));
    assert(!pd_hr_bench_should_send(&b, 9001, true, false, false));
    assert(!pd_hr_bench_should_send(&b, 9100, true, true, true));
    pd_hr_bench_init(&b);
    /* An interrupted renegotiation cannot count as 500ms uninterrupted. */
    assert(!pd_hr_bench_should_send(&b, 5, true, false, false));
    assert(!pd_hr_bench_should_send(&b, 10, true, true, true));
    assert(!pd_hr_bench_should_send(&b, 250, true, false, true));
    assert(!pd_hr_bench_should_send(&b, 500, true, true, true));
    assert(!pd_hr_bench_should_send(&b, 999, true, true, true));
    assert(pd_hr_bench_should_send(&b, 1000, true, true, true));
    pd_hr_bench_init(&b);
    /* Rollover-safe timer. */
    assert(!pd_hr_bench_should_send(&b, 0xffffff00u, true, false, false));
    assert(!pd_hr_bench_should_send(&b, 0xffffff80u, true, true, true));
    assert(!pd_hr_bench_should_send(&b, 0xffffff80u+499u, true, true, true));
    assert(pd_hr_bench_should_send(&b, 0xffffff80u+500u, true, true, true));
    puts("M13a-5 bench gate: PASS (one-shot, re-READY, ADC busy, rollover)");
    return 0;
}
