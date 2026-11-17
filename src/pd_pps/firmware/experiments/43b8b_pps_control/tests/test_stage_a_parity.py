"""Verify M43B-8 policy behavior against the exact M43B-7 P2 source."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BASE_SHA = 'db71eb11e3237cafe94437ce27c9dfb56aaa96f6a87052f60de749a7e380aeec'
SYMBOLS = ('pe_init','pe_event','pe_tick','pe_command_result',
           'pe_request_soft_reset','pe_request_source_caps')

class PolicyStageAParity(unittest.TestCase):
    def test_new_policy_vs_pinned_p2(self):
        cc = shutil.which('gcc') or shutil.which('clang')
        if not cc:
            self.skipTest('gcc/clang unavailable')
        baseline=ROOT/'tests/fixtures/pd_policy_m43b7_p2.c'
        self.assertEqual(hashlib.sha256(baseline.read_bytes()).hexdigest(),BASE_SHA)
        with tempfile.TemporaryDirectory(prefix='m43b8-parity-') as d:
            tmp=Path(d)
            objects=[tmp/'baseline.o',tmp/'current.o',tmp/'power.o']
            exe=tmp/('parity.exe' if os.name=='nt' else 'parity')
            flags=[cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src')]
            def check(args):
                r=subprocess.run(args,check=True,capture_output=True,text=True)
                return r.stdout
            check([*flags,*[f'-D{s}=baseline_{s}' for s in SYMBOLS],
                   '-c',str(baseline),'-o',str(objects[0])])
            check([*flags,'-c',str(ROOT/'src/pd_policy.c'),'-o',str(objects[1])])
            check([*flags,'-c',str(ROOT/'src/pd_power.c'),'-o',str(objects[2])])
            check([*flags,*map(str,objects),
                   str(ROOT/'tests/test_stage_a_policy_parity_runtime.c'),
                   '-o',str(exe)])
            self.assertIn('POLICY_PARITY_PASS',check([str(exe)]))
