"""Change scope remains P0/P1; lock down ISR and timing-sensitive sources."""
from pathlib import Path
import hashlib
import json
import unittest
ROOT=Path(__file__).resolve().parents[1]
class ScopeGuard(unittest.TestCase):
    def test_all_unmodified_sources_match_original(self):
        mapping=json.loads((ROOT/'tests/fixtures/unchanged_m43b5_v01_sha256.json').read_text())
        self.assertGreaterEqual(len(mapping),38)
        for filename, expected in mapping.items():
            with self.subTest(filename=filename):
                self.assertEqual(hashlib.sha256((ROOT/filename).read_bytes()).hexdigest(),expected)
    def test_only_policy_and_cdc_changed_and_no_new_isr_work(self):
        policy=(ROOT/'src/pd_policy.c').read_text()
        cdc=(ROOT/'src/m38_usb.c').read_text()
        self.assertIn('enter_recovery_after_power_loss',policy)
        self.assertEqual(policy.count('enter_recovery_after_power_loss(p,'),2)
        self.assertIn('parse_bounded_index',cdc)
        self.assertEqual(cdc.count('parse_bounded_index(command +'),2)
        irq=cdc[cdc.index('void m38_usb_on_irq'):cdc.index('void HandleDataOut')]
        self.assertNotIn('parse_bounded_index',irq)
        self.assertNotIn('phy_',irq)
