from pathlib import Path
import subprocess, tempfile, shutil, unittest
ROOT=Path(__file__).resolve().parents[1]
class M38CHost(unittest.TestCase):
    def test_cdc_host_runtime(self):
        cc=shutil.which('gcc') or shutil.which('clang')
        if not cc: self.skipTest('Native gcc/clang unavailable; run C runtime test on a machine with a native compiler')
        with tempfile.TemporaryDirectory(prefix='m38a-test-') as d:
            exe=Path(d)/('test_m38.exe' if __import__('os').name=='nt' else 'test_m38')
            subprocess.run([cc,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'tests/stubs'),'-I'+str(ROOT/'src'),
                           str(ROOT/'src/m38_usb.c'),str(ROOT/'tests/test_m38_usb_runtime.c'),'-o',str(exe)],check=True,capture_output=True,text=True)
            result=subprocess.run([str(exe)],check=True,capture_output=True,text=True)
            self.assertIn('M40 host C CDC test PASS',result.stdout)
