"""Fast C-host test of the independent PDO/APDO planner."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

class PowerPlannerTests(unittest.TestCase):
    def test_pure_power_planner(self):
        cc=shutil.which('gcc') or shutil.which('clang')
        if not cc:
            self.skipTest('gcc or clang unavailable')
        with tempfile.TemporaryDirectory(prefix='m43b8-power-') as d:
            exe=Path(d)/('test.exe' if os.name=='nt' else 'test')
            cmd=[cc,'-std=c11','-O2','-Wall','-Wextra','-Werror',
                 '-I'+str(ROOT/'src'),str(ROOT/'src/pd_power.c'),
                 str(ROOT/'tests/test_pd_power_runtime.c'),'-o',str(exe)]
            subprocess.run(cmd,check=True,capture_output=True,text=True)
            ran=subprocess.run([str(exe)],check=True,capture_output=True,text=True)
            self.assertIn('POWER_SELECT_PASS',ran.stdout)
