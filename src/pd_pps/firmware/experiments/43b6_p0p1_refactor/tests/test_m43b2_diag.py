from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class M43B2Diagnostics(unittest.TestCase):
    def test_phy_instrumentation_and_no_change_to_controls(self):
        phy = (ROOT/'src/pd_phy_ch32x035.c').read_text(encoding='utf-8')
        self.assertIn('m43b2_diag_pd_irq(status)', phy)
        self.assertIn('m43b2_diag_goodcrc(match, crc_wait_armed', phy)
        self.assertIn('m43b2_diag_rx_arm(', phy)
        self.assertIn('m43b2_diag_attach()', phy)
        self.assertIn('m43b2_diag_detach()', phy)
        self.assertIn('m43b2_diag_tx_failed()', phy)
        self.assertNotIn('trace_add(PDTR_M41_', phy)
        self.assertIn('USBPD->STATUS = IF_TX_END; /* W1C */', phy)
    def test_usb_irq_only_passively_reads_pd_window(self):
        fs = (ROOT/'m28_fsusb.c').read_text(encoding='utf-8')
        self.assertIn('m43b2_diag_usbfs_enter(phy_crc_window_open(), SysTick->CNTL)',fs)
        self.assertIn('m43b2_diag_usbfs_exit(SysTick->CNTL)',fs)
        self.assertNotIn('phy_send(',fs)
        self.assertNotIn('USBPD->STATUS =',fs)
    def test_no_printf_in_irq_diagnostics(self):
        diag = (ROOT/'src/m43b2_diag.c').read_text(encoding='utf-8')
        self.assertNotIn('printf(',diag)
        self.assertNotIn('USBPD->',diag)
        self.assertNotIn('USBFS->',diag)
    def test_size_and_linkage(self):
        mk=(ROOT/'Makefile').read_text(encoding='utf-8')
        self.assertIn('src/m43b2_diag.c',mk)
        self.assertIn('TARGET := m43b6_p0p1_refactor',mk)
        self.assertIn('PD17 M43B2 IRQ pd=',(ROOT/'src/main.c').read_text())
    def test_native_counter_semantics(self):
        cc = shutil.which('gcc') or shutil.which('clang')
        if not cc: self.skipTest('C compiler not installed')
        with tempfile.TemporaryDirectory(prefix='m43b2diag_') as td:
            exe = Path(td)/('diag.exe' if __import__('os').name=='nt' else 'diag')
            result=subprocess.run([cc,'-std=c11','-Wall','-Wextra','-Werror',
                                   '-I'+str(ROOT/'src'),
                                   str(ROOT/'src/m43b2_diag.c'),
                                   str(ROOT/'tests/test_m43b2_diag_runtime.c'),
                                   '-o',str(exe)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stderr)
            self.assertIn('M43B2 DIAG HOST PASS',run.stdout)
if __name__ == '__main__': unittest.main()
