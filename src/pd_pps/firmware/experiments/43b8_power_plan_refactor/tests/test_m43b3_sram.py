from pathlib import Path
import importlib.util
import hashlib
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('m43b4_decoder',ROOT/'Decode-M43B5-SRAM.py')
mod=importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

class SramHostTests(unittest.TestCase):
    def test_runtime_native(self):
        cc=shutil.which('gcc') or shutil.which('clang')
        if not cc: self.skipTest('No native C compiler')
        with tempfile.TemporaryDirectory(prefix='m43b3c_') as tmp:
            exe=Path(tmp)/('test.exe' if sys.platform=='win32' else 'test')
            cmd=[cc,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),
                 str(ROOT/'src/m43b3_sram.c'),str(ROOT/'tests/test_m43b3_sram_runtime.c'),'-o',str(exe)]
            r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            self.assertEqual(r.returncode,0,r.stderr)
            r=subprocess.run([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            self.assertEqual(r.returncode,0,r.stdout+r.stderr)
            self.assertIn('M43B3 SRAM HOST PASS',r.stdout)
    def test_structural_hooks(self):
        phy=(ROOT/'src/pd_phy_ch32x035.c').read_text(encoding='utf-8')
        self.assertIn('m43b3_sram_attach(SysTick->CNTL)',phy)
        self.assertIn('m43b3_sram_freeze(M43B3_FREEZE_TX_FAILED',phy)
        self.assertIn('m43b3_sram_freeze(M43B3_FREEZE_SOURCE_HR',phy)
        self.assertIn('m43b3_sram_note(M43B3_EVT_GOODCRC',phy)
        self.assertIn('m43b3_sram_detach(SysTick->CNTL)',phy)
        mk=(ROOT/'Makefile').read_text(encoding='utf-8')
        self.assertIn('src/m43b3_sram.c',mk)
        self.assertIn('TARGET := m43b8_power_plan_refactor',mk)
        sram=(ROOT/'src/m43b3_sram.c').read_text(encoding='utf-8')
        self.assertNotIn('printf(',sram)
        self.assertNotIn('USBPD->',sram)
    def _raw(self):
        raw=bytearray(mod.RAM_BYTES)
        offset=0x100
        vals=(mod.MAGIC,1,64,16,64,2,3,3,3,1,100,0,0,1,0,mod.END_MAGIC)
        struct.pack_into('<16I',raw,offset,*vals)
        for i in range(3):
            struct.pack_into('<IIIHBB',raw,offset+64+i*16,100+i,0x1234+i,0x5678+i,i,1,12)
        return raw,mod.RAM_BASE+offset
    def test_decoder_valid(self):
        raw,addr=self._raw()
        d,rows=mod.decode(raw,addr)
        self.assertEqual(d['freeze_reason'],1)
        self.assertEqual(len(rows),3)
        self.assertEqual([r['seq'] for r in rows],[0,1,2])
    def test_decoder_rejects_unknown_signature(self):
        raw,addr=self._raw()
        struct.pack_into('<I',raw,0x100,0)
        with self.assertRaisesRegex(ValueError,'signature'):mod.decode(raw,addr)
    def test_decoder_rejects_seq_mismatch(self):
        raw,addr=self._raw()
        struct.pack_into('<H',raw,0x100+64+16*2+12,9)
        with self.assertRaisesRegex(ValueError,'sequence'):mod.decode(raw,addr)
    def test_decoder_wrapped_order(self):
        raw,addr=self._raw()
        off=0x100
        struct.pack_into('<I',raw,off+6*4,2) # head
        struct.pack_into('<I',raw,off+7*4,64) # count
        struct.pack_into('<I',raw,off+8*4,66) # total
        for i in range(64):
            seq=(i+2) if i<=1 else i-62
            # slot 2 is oldest sequence 2, slot 1 is newest sequence 65
            seq=(i+64 if i<2 else i) & 0xffff
            struct.pack_into('<IIIHBB',raw,off+64+i*16,i,i,i,seq,2,0)
        _,rows=mod.decode(raw,addr)
        self.assertEqual(rows[0]['slot'],2)
        self.assertEqual(rows[-1]['slot'],1)
        self.assertEqual(rows[0]['seq'],2)
        self.assertEqual(rows[-1]['seq'],65)
    def test_decoder_stops_on_wrong_length(self):
        raw,addr=self._raw()
        with self.assertRaisesRegex(ValueError,'Expected'):mod.decode(raw[:-1],addr)
    def test_decoder_cli_no_overwrite(self):
        raw,addr=self._raw()
        with tempfile.TemporaryDirectory(prefix='m43b3decode_') as tmp:
            tmp=Path(tmp); mem=tmp/'ram.bin'; mem.write_bytes(raw); out=tmp/'out'
            cmd=[sys.executable,str(ROOT/'Decode-M43B5-SRAM.py'),
                 '--ram',str(mem),'--address',hex(addr),'--out',str(out)]
            r=subprocess.run(cmd,capture_output=True,text=True)
            self.assertEqual(r.returncode,0,r.stdout+r.stderr)
            self.assertTrue((out/'events.csv').is_file())
            self.assertTrue((out/'summary.json').is_file())
            h=hashlib.sha256(mem.read_bytes()).hexdigest()
            r=subprocess.run(cmd,capture_output=True,text=True)
            self.assertNotEqual(r.returncode,0)
            self.assertEqual(hashlib.sha256(mem.read_bytes()).hexdigest(),h)
if __name__=='__main__':unittest.main()
