from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
root=Path(__file__).resolve().parents[1]
def read(p): return (root/p).read_text(encoding='utf-8')
class M36Source(unittest.TestCase):
    def test_target_and_flags(self):
        mk=read('Makefile')
        self.assertIn('TARGET := m36_pd_usbfs_pullup_isolation',mk)
        self.assertIn('src/m36_pullup.c',mk)
        self.assertNotIn('src/m35_phy.c',mk)
        self.assertIn('-Os -flto',mk)
    def test_pullup_only_phase(self):
        s=read('src/m36_pullup.c')
        between=s.split('static void disable_only_dp_pullup(void) {',1)[1].split('}',1)[0].split('*/')[-1]
        self.assertIn('AFIO->CTLR &= ~((uint32_t)UDP_PUE_MASK);',between)
        for forbidden in ('USBFS->','GPIOC->','USBPD->','NVIC_', 'RCC->', 'USB_IOEN', 'UDM_PUE_MASK'):
            self.assertNotIn(forbidden,between)
    def test_all_off_c_phase_preserves_pd(self):
        s=read('src/m36_pullup.c')
        self.assertIn('USBFS->BASE_CTRL=0;',s)
        self.assertIn('USBFS->UDEV_CTRL=USBFS_UD_PD_DIS;',s)
        self.assertIn('GPIOC->CFGXR & ~0xffu',s)
        self.assertIn('NVIC_DisableIRQ(USB_IRQn)',s)
        for forbidden in ('USBPD->','RCC->','GPIOC->CFGLR ='):
            self.assertNotIn(forbidden,s)
    def test_30_30_second_and_restore(self):
        s=read('src/m36_pullup.c')
        for marker in ('#define M36_A_MS 30000u','#define M36_B_MS 30000u', '#define M36_RESTORE_MS 500u','(void)USBFSSetup();','stage=ST_NO_PULLUP;', 'stage=ST_PHY_OFF;'):
            self.assertIn(marker,s)
    def test_integration_and_reply(self):
        main=read('src/main.c')
        readonly=read('src/m5_readonly.c')
        for marker in ('m36_pullup_init();','m36_pullup_tick(now, phy_attached()', 'm36_pullup_usb_service_blocked()', 'm36_pullup_led_interval_ms()'):
            self.assertIn(marker,main)
        self.assertIn('m36_pullup_reply(body,length,out)',readonly)
        self.assertNotIn('m35_phy_init()',main)
    def test_read_script(self):
        s=read('Read-M36-Diag.ps1')
        self.assertIn('PUDIAG',s)
        self.assertIn('ReadLine()',s)
    def test_mock_runtime(self):
        cc=shutil.which('gcc') or shutil.which('clang')
        if not cc:self.skipTest('No GCC/Clang for mock target')
        with tempfile.TemporaryDirectory() as td:
            exe=Path(td)/('m36_test.exe' if os.name=='nt' else 'm36_test')
            subprocess.run([cc,'-std=c11','-Wall','-Wextra','-Werror','-Itests/m36_stubs','-Isrc',
                            'tests/test_m36_runtime.c','src/m36_pullup.c','-o',str(exe)],cwd=root,check=True,capture_output=True,text=True)
            p=subprocess.run([str(exe)],cwd=root,check=True,capture_output=True,text=True)
            self.assertIn('M36 mocked runtime PASS',p.stdout)
