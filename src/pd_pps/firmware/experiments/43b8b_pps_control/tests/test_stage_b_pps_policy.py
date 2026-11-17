"""Native stage-B policy integration tests. No connected hardware required."""
from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[1]
class StageBPpsPolicy(unittest.TestCase):
    def test_transition_and_safety(self):
        cc=shutil.which('gcc') or shutil.which('clang')
        if not cc: self.skipTest('gcc/clang not available')
        with tempfile.TemporaryDirectory(prefix='m43b8b-') as tmp:
            exe=Path(tmp)/('stage_b.exe' if os.name=='nt' else 'stage_b')
            cmd=[cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),
                 str(ROOT/'src/pd_policy.c'),str(ROOT/'src/pd_power.c'),
                 str(ROOT/'tests/test_stage_b_pps_policy.c'),'-o',str(exe)]
            subprocess.run(cmd,check=True,capture_output=True,text=True)
            r=subprocess.run([str(exe)],check=True,capture_output=True,text=True)
            self.assertIn('STAGE_B_PPS_POLICY_PASS',r.stdout)
if __name__=='__main__': unittest.main()
