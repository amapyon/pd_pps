from pathlib import Path
import unittest
import shutil
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
read=lambda p:(root/p).read_text(encoding="utf-8")
class M35PHY(unittest.TestCase):
    def test_isolated_directory_and_target(self):
        mk=read('Makefile')
        self.assertIn('TARGET := m35_pd_usbfs_phy_isolation',mk)
        self.assertIn('src/m35_phy.c',mk)
        self.assertIn('-Os -flto',mk)
    def test_safe_gpio_and_phy_boundary(self):
        s=read('src/m35_phy.c')
        self.assertIn('NVIC_DisableIRQ(USB_IRQn)',s)
        self.assertIn('USBFS->INT_EN=0;',s)
        self.assertIn('USBFS->UDEV_CTRL=USBFS_UD_PD_DIS;',s)
        self.assertIn('USBFS->BASE_CTRL=0;',s)
        self.assertIn('~(USB_IOEN | UDP_PUE_MASK | UDM_PUE_MASK)',s)
        self.assertIn('GPIOC->CFGXR & ~0xffu',s)
        for forbidden in ('USBPD->', 'phy_send(', 'pe_event(', 'RCC->', 'GPIOC->CFGLR ='):
            self.assertNotIn(forbidden,s)
    def test_restore_only_after_detach(self):
        s=read('src/m35_phy.c')
        self.assertIn('#define M35_BASELINE_MS 30000u',s)
        self.assertIn('#define M35_RESTORE_MS 500u',s)
        self.assertIn('if(!pd_attached) {',s)
        self.assertIn('(void)USBFSSetup();',s)
        self.assertIn('stage=ST_DONE;',s)
    def test_integration(self):
        s=read('src/main.c')
        self.assertIn('m35_phy_init();',s)
        self.assertIn('m35_phy_tick(now, phy_attached()',s)
        self.assertIn('if (!m35_phy_is_off())',s)
        self.assertIn('m35_phy_is_off() ? 100u : 250u',s)
        self.assertIn('m34_diag_tick(now, phy_attached()',s)
        self.assertIn('m35_phy_reply(body,length,out)',read('src/m5_readonly.c'))
    def test_read_script(self):
        s=read('Read-M35-Diag.ps1')
        self.assertIn('PHYDIAG',s)
        self.assertIn('ReadLine()',s)

class M35Runtime(unittest.TestCase):
    def test_c_runtime_when_host_compiler_present(self):
        cc=shutil.which('gcc') or shutil.which('clang')
        if not cc: self.skipTest('No host C compiler; use source checks + target build')
        with tempfile.TemporaryDirectory() as td:
            exe=Path(td)/('m35_test.exe' if __import__('os').name=='nt' else 'm35_test')
            cmd=[cc,'-std=c11','-Wall','-Wextra','-Werror','-Itests/m35_stubs','-Isrc',
                 'tests/test_m35_runtime.c','src/m35_phy.c','-o',str(exe)]
            subprocess.run(cmd, check=True, cwd=root, capture_output=True, text=True)
            proc=subprocess.run([str(exe)],check=True,cwd=root,capture_output=True,text=True)
            self.assertIn('M35 mocked runtime PASS',proc.stdout)
