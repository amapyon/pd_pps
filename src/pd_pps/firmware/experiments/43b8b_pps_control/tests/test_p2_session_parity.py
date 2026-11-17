"""Safety-oriented structural parity for main.c -> session/diagnostics split."""
from pathlib import Path
import hashlib
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
PINNED = ROOT / 'tests/fixtures/main_m43b6_p0p1.c'
MAIN = ROOT / 'src/main.c'
SESSION = ROOT / 'src/pd_session.c'
DIAG = ROOT / 'src/pd_session_diag.c'


def read(p): return p.read_text(encoding='utf-8')


def printf_formats(s):
    # Includes C-escaped newlines; escaped quotes are not used in baseline.
    return re.findall(r'\bprintf\("((?:[^"\\]|\\.)*)"', s)


class P2SessionRefactor(unittest.TestCase):
    def test_pinned_m43b6_main_is_immutable(self):
        self.assertEqual(hashlib.sha256(PINNED.read_bytes()).hexdigest(),
                         'd102241a5edec251dcd929deabd0c0617b6a38ee88208de7d79c60c48abe5607')

    def test_diagnostic_output_format_and_order_are_exactly_preserved(self):
        self.assertEqual(printf_formats(read(PINNED)),
                         printf_formats(read(DIAG)))

    def test_no_sync_output_in_attached_critical_session(self):
        self.assertNotIn('printf(', read(SESSION))
        self.assertNotIn('printf(', read(MAIN))
        self.assertIn('if (trace_dump_pending && !phy_attached()', read(DIAG))

    def test_each_foreground_primitive_retained_once(self):
        old = read(PINNED)
        new = read(SESSION)
        for expr in ('phy_poll_cc(now);', 'pd_vbus_monitor_tick(now,',
                     'phy_abort_for_vbus_loss(raw, adc_voltage_vbus_mv(raw))',
                     'phy_abort_for_adc_failure()', 'phy_maintain(now);',
                     'phy_pop(&ev, &caps)', 'pd_hr_recovery_step(',
                     'pe_tick(&policy,now);', 'phy_send(cmd,policy.rdo,now);',
                     'm42_diag_capture_first_stop(&m13c2_stop);',
                     'm38_usb_service(now, &policy, phy_attached(),'):
            with self.subTest(expr=expr):
                self.assertEqual(old.count(expr), 1)
                self.assertEqual(new.count(expr), 1)

    def test_phase_order_and_cdc_after_pd(self):
        s = read(SESSION)
        start = s.index('void pd_session_step(uint32_t now) {')
        steps = s[start:]
        order = ['session_safety_and_phy(now);',
                 'session_receive_events(now);',
                 'session_hr_recovery(now);',
                 'session_advance_policy(now);',
                 'session_sensors_and_tx(now);',
                 'session_cdc_after_pd(now);',
                 'pd_diag_report_step(now,',
                 'pd_diag_heartbeat(now,']
        offsets = [steps.index(x) for x in order]
        self.assertEqual(offsets, sorted(offsets))
        self.assertIn('phy_m43b5_mark_refresh()', s)
        self.assertIn('pd_stability_tick(&m13c_stats, now,', s)

    def test_initialization_and_irq_order(self):
        main = read(MAIN)
        for text in ['SystemInit();','SysTick->CTLR =',
                     'pd_session_init_core();',
                     'NVIC_SetPriority(USBPD_IRQn, 0x00u);',
                     'NVIC_SetPriority(USBFS_IRQn, 0x80u);',
                     'm38_usb_init();',
                     'funDigitalWrite(PB12, FUN_LOW);',
                     'pd_session_init_state();']:
            self.assertIn(text, main)
        assert_order = ['pd_session_init_core();',
                        'NVIC_SetPriority(USBPD_IRQn, 0x00u);',
                        'NVIC_SetPriority(USBFS_IRQn, 0x80u);',
                        'm38_usb_init();','funDigitalWrite(PB12, FUN_LOW);',
                        'pd_session_init_state();']
        offsets = [main.index(x) for x in assert_order]
        self.assertEqual(offsets, sorted(offsets))
        s = read(SESSION)
        for x in ('pe_init(&policy);','phy_init();','pd_sensors_init();',
                  'pd_vbus_monitor_init();','m42_diag_new_attach();'):
            self.assertIn(x,s)

    def test_dump_cleanup_order(self):
        s=read(SESSION)
        starts=s.index('if (pd_diag_report_step(')
        completed=s[starts:s.index('pd_diag_heartbeat(now,',starts)]
        order=['pd_sensors_clear();','pd_vbus_monitor_clear();',
               'sensor_ready_seen = false;',
               'sensor_ready_sampled = false;',
               'pd_diag_finish_cleanup();']
        indices=[completed.index(x) for x in order]
        self.assertEqual(indices, sorted(indices))
        self.assertIn('pd_vbus_monitor_report_step()',read(DIAG))

    def test_p1_and_phy_modules_byte_identical_to_verified_m43b6(self):
        import json
        pinned=json.loads((ROOT/'tests/fixtures/p2_pinned_sensitive_sha256.json').read_text())
        for filename,sha in pinned.items():
            if filename in ('src/pd_policy.c', 'src/m38_usb.c'):
                # Intentional M43B-8 A policy and M43B-8 B CDC source changes.
                continue
            with self.subTest(filename=filename):
                self.assertEqual(hashlib.sha256((ROOT/filename).read_bytes()).hexdigest(),sha)

    def test_no_unintended_phy_usb_or_sram_changes(self):
        import json
        allhash=json.loads((ROOT/'tests/fixtures/unchanged_m43b5_v01_sha256.json').read_text())
        for file,sha in allhash.items():
            if file in ('src/main.c', 'src/pd_policy.c', 'src/pd_policy.h', 'src/m38_usb.h'): continue
            self.assertEqual(hashlib.sha256((ROOT/file).read_bytes()).hexdigest(),sha,file)


if __name__ == '__main__': unittest.main()
