from pathlib import Path
import subprocess, tempfile, shutil, unittest, os
ROOT=Path(__file__).resolve().parents[1]
class M38BCHost(unittest.TestCase):
    def test_cdc_host_runtime(self):
        cc=shutil.which('gcc') or shutil.which('clang')
        if not cc: self.skipTest('No native C compiler; C runtime test skipped')
        with tempfile.TemporaryDirectory(prefix='m38b-test-') as d:
            exe=Path(d)/('test_m38b.exe' if os.name=='nt' else 'test_m38b')
            r=subprocess.run([cc,'-std=c11','-Wall','-Wextra','-Werror',
                '-I'+str(ROOT/'tests/stubs'), '-I'+str(ROOT/'src'),
                str(ROOT/'src/m38b_usb.c'),str(ROOT/'tests/test_m38b_usb_runtime.c'),
                '-o',str(exe)], capture_output=True,text=True)
            if r.returncode: self.fail('Host C compiler failed: '+r.stderr)
            r=subprocess.run([str(exe)], capture_output=True,text=True)
            if r.returncode: self.fail('Host C test failed: '+r.stdout+r.stderr)
            self.assertIn('M38B host C CDC test PASS',r.stdout)
