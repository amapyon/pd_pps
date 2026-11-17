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
        # keep PD17/M13b PHY baseline fixed, with M40 snapshot API added
        expected={
          'pd_vbus_monitor.c':'036e6e5588d1336646a5edff14b8e08dad9878858ad29050b0330b3fce9899e2',
          'ina226.c':'9c7407783abb7d9e447d037a10d28f7b5520d3d839d903e42cfc4384aa136846',
          'adc_voltage.c':'934b53877359eeafd2905dbb550dc953967c21afb337f8039f5fcd69bd41937a',
        }
        for filename,sha in expected.items():
            raw=(ROOT/'src'/filename).read_bytes()
            got=hashlib.sha256(raw).hexdigest()
            self.assertEqual(got,sha,filename)
if __name__=='__main__':unittest.main()
