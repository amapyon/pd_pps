from pathlib import Path
import unittest, hashlib
ROOT=Path(__file__).resolve().parents[1]
class M42SnapshotStructure(unittest.TestCase):
    def test_snapshot_only_after_first_stop(self):
        txt=(ROOT/'src/m42_diag.c').read_text()
        self.assertIn('stop->active',txt)
        self.assertIn('stop->found',txt)
        self.assertIn('pd_vbus_history_get(',txt)
        self.assertNotIn('adc_voltage_start_async',txt)
        self.assertNotIn('ina226_read(',txt)
    def test_original_pd_source_unmodified(self):
        # keep safety/PHY unchanged vs pinned M41 SHA-256
        expected={
          'pd_phy_ch32x035.c': '64006edc3690e562b92ca9552cabcc815525571d5ba0b0ae41e230e626892eca',
          'pd_policy.c':'a0700b80d38c1afa6dec56ebc30a91714bba218e4e4691a9e8af597c1a7e9be3',
          'pd_vbus_monitor.c':'036e6e5588d1336646a5edff14b8e08dad9878858ad29050b0330b3fce9899e2',
          'ina226.c':'9c7407783abb7d9e447d037a10d28f7b5520d3d839d903e42cfc4384aa136846',
          'adc_voltage.c':'934b53877359eeafd2905dbb550dc953967c21afb337f8039f5fcd69bd41937a',
        }
        for filename,sha in expected.items():
            got=hashlib.sha256((ROOT/'src'/filename).read_bytes()).hexdigest()
            self.assertEqual(got,sha,filename)
if __name__=='__main__':unittest.main()
