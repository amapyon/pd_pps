"""M43B-8 B intentional deltas: pin changed PE/CDC files explicitly.

Older P0/P2 historical hashes remain enforced for all files not intentionally
modified in A (policy extraction) or B (PPS CDC control).
"""
from pathlib import Path
import hashlib
import unittest

ROOT = Path(__file__).resolve().parents[1]
EXPECTED_SHA256 = {'src/pd_policy.c': '23f83d9b3effe970f018dc38e5cb35ac907869c8b361f6537eccd9e4e80946ca', 'src/pd_policy.h': '7338deaeae44e28ba8663abb59bc621456f33a608b963114f558df45e66c00f4', 'src/pd_power.c': '1e17b11c2758729e553aecf6fe37cd0c90bc76ea2b619d224841fc60534814f0', 'src/pd_power.h': 'c9120b590229851575235a1e138d5735141f0849b84d6064adb3ab79f316e0db', 'src/m38_usb.c': '4410afcc65421948ab5bfc56bc2f7c69664e76ee9b1230a51a86e8aa28defdfc', 'src/m38_usb.h': '1993c03f008d944f5ea829ae548c4395b172bd5aebd33e3fca5c542e65a13d04'}

class M43B8BScope(unittest.TestCase):
    def test_intentionally_modified_sources_are_pinned_to_b_v01(self):
        for filename, expected in EXPECTED_SHA256.items():
            with self.subTest(filename=filename):
                got = hashlib.sha256((ROOT / filename).read_bytes()).hexdigest()
                self.assertEqual(got, expected)

    def test_usb_irq_still_does_not_run_pd_policy(self):
        usb = (ROOT / 'src/m38_usb.c').read_text(encoding='utf-8')
        irq = usb[usb.index('void m38_usb_on_irq'):usb.index('void HandleDataOut')]
        self.assertNotIn('pe_set_pps_target', irq)
        self.assertNotIn('phy_send', irq)
        self.assertIn('pe_set_pps_target(pe, mv, ma, now_ms)', usb)
        self.assertIn('parse_pps_pair(command + 8u, &mv, &ma)', usb)
        self.assertIn('PPS STATUS', usb)
        session = (ROOT / 'src/pd_session.c').read_text(encoding='utf-8')
        self.assertLess(session.index('session_safety_and_phy(now);'),
                        session.index('session_cdc_after_pd(now);'))

    def test_new_target_uses_distinct_wrapper(self):
        mk = (ROOT / 'Makefile').read_text(encoding='utf-8')
        wrapper = (ROOT / 'm43b8b_pps_control.c').read_text(encoding='utf-8')
        self.assertIn('TARGET := m43b8b_pps_control', mk)
        self.assertIn('#include "src/main.c"', wrapper)

if __name__ == '__main__':
    unittest.main()
