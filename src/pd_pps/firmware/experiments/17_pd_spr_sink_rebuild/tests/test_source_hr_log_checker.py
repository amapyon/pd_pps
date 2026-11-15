#!/usr/bin/env python3
"""Self-contained tests for M13d detached-only PD17 trace assessment."""
import unittest
from check_source_hr_log import examine


def sample(hr_count=0, stop_reason=1, pps=True, complete=True,
           lost=0, error=0, ready_after_second=False):
    lines = ["PD17: alive, charger detached, state=0, lost=0",
             "PD17 TRACE BEGIN (tick=SysTick CNTL, low 32 bits)",
             "PD17 TRACE tick=10000000 code=1 detail=1 aux=e009 value=00000000",
             "PD17 TRACE tick=10000001 code=15 detail=4 aux=0003 value=00000004"]
    for i in range(hr_count):
        lines.extend([
            f"PD17 TRACE tick=1000000{i+2} code=3 detail=0 aux=0000 value=00000000",
            f"PD17 TRACE tick=1000001{i+2} code=22 detail=0 aux=0000 value=00000000",
            f"PD17 TRACE tick=1000002{i+2} code=23 detail=0 aux=0000 value=00000000",
        ])
        if i == 0 or ready_after_second:
            lines.append(f"PD17 TRACE tick=1000003{i+2} code=15 detail=4 aux=0003 value=00000004")
    lines.extend([
        "PD17 TRACE tick=20000001 code=18 detail=0 aux=0013 value=000000a8",
        "PD17 TRACE tick=20000002 code=2 detail=1 aux=0000 value=00000000",
        f"PD17 TRACE END trace_dropped=0 event_lost={lost} state=0 faults=0",
        f"PD17 STABILITY session_ms=25000 ready_ms=24500 loop_gap_max_ms=2 caps=1 requests=6 pps_ready={1+hr_count if pps else 0} fixed_ready={0 if pps else 1+hr_count} refresh_started=4 refresh_done=4",
        f"PD17 STABILITY ERR source_hr={hr_count} pe_recovery=1 vbus_loss=1 adc_fault=0 policy_faults=1 trace_drop_session=0 event_lost_session={lost}",
        f"PD17 STOP reason={stop_reason} first_rel_attach_ms=24870 pe_before=4 pe_after=6 aux=20 hr_rx={hr_count} detach_seen=1 detach_after_stop_ms=130",
        "PD17 MODE normal=1 bench_hr_tx=0 bench_get_caps=0 pps_reprobe=0 hr_rx_max_per_attach=1 m13c_stats=1 adc_fail_stop=1 m13c2_stop_diag=1 hr_gate_budget_ms=20",
        f"PD17 HR VBUS state={3 if hr_count else 0} error={error} low_seen={1 if hr_count else 0} low_raw=19 high_seen={1 if hr_count else 0} high_raw=570 low_rel_hr_ms=60 high_rel_hr_ms=915 recovery_handoff={1 if hr_count else 0} auto_recovery={1 if hr_count else 0} pps_recovery={1 if hr_count and pps else 0}",
    ])
    if complete:
        lines.append("PD17 VBUS END")
    return "\n".join(lines)


class SourceHrLogCheckerTests(unittest.TestCase):
    def check(self, data, mode, want, expected_ready="either"):
        result, details = examine(data, mode, expected_ready)
        self.assertEqual(result, want, details)

    def test_baseline(self):
        self.check(sample(), "baseline", "PASS")

    def test_first_pps(self):
        self.check(sample(hr_count=1), "first", "PASS", "pps")

    def test_first_fixed(self):
        self.check(sample(hr_count=1, pps=False), "first", "PASS", "fixed")

    def test_second_fail_closed(self):
        self.check(sample(hr_count=2, stop_reason=10), "second", "PASS")

    def test_second_unwanted_recovery(self):
        self.check(sample(hr_count=2, stop_reason=10, ready_after_second=True), "second", "FAIL")

    def test_second_wrong_stop(self):
        self.check(sample(hr_count=2), "second", "FAIL")

    def test_normal_detach_not_hr(self):
        self.check(sample(), "first", "FAIL")

    def test_incomplete(self):
        self.check(sample(hr_count=1, complete=False), "first", "INCOMPLETE")

    def test_lost_events(self):
        self.check(sample(hr_count=1, lost=1), "first", "INCOMPLETE")

    def test_hr_voltage_error(self):
        self.check(sample(hr_count=1, error=1), "first", "FAIL")

    def test_session_selection(self):
        self.check(sample() + "\n" + sample(hr_count=1), "first", "PASS")

    def test_sink_tx_must_not_occur(self):
        tampered = sample(hr_count=1).replace(
            "PD17 TRACE tick=10000002 code=3", "PD17 TRACE tick=10000009 code=19 detail=0 aux=0000 value=00000000\nPD17 TRACE tick=10000002 code=3")
        self.check(tampered, "first", "FAIL")


if __name__ == "__main__":
    unittest.main()
