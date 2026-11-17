from pathlib import Path
import importlib.util, tempfile, subprocess, sys, unittest
ROOT=Path(__file__).resolve().parents[1]
class M41Decoder(unittest.TestCase):
    def test_decode_register_and_context_records(self):
        fake="PDTRACE idx=0 count=2 tick=00000000 code=29 detail=6 aux=8081 value=0003e00c\nPDTRACE idx=1 count=2 tick=0000bb80 code=33 detail=0 aux=0100 value=0000bb80\n"
        with tempfile.TemporaryDirectory() as td:
            path=Path(td)/'in.txt';path.write_text(fake)
            p=subprocess.run([sys.executable,str(ROOT/'Decode-M43A-Trace.py'),str(path)],capture_output=True,text=True,check=True)
            self.assertIn('CONTROL(R8)=0x03',p.stdout)
            self.assertIn('CONFIG(R16)=0xe00c',p.stdout)
            self.assertIn('expected_mid=0',p.stdout)
            self.assertIn('1.000ms',p.stdout)
if __name__=='__main__': unittest.main()
