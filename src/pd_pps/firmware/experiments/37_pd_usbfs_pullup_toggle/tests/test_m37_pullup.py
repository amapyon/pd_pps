from pathlib import Path
import os, shutil, subprocess, tempfile, unittest
root=Path(__file__).resolve().parents[1]
def read(p): return (root/p).read_text(encoding='utf-8')
class M37Source(unittest.TestCase):
    def test_target_and_flags(self):
        mk=read('Makefile')
        self.assertIn('TARGET := m37_pd_usbfs_pullup_toggle',mk)
        self.assertIn('src/m37_pullup.c',mk)
        self.assertNotIn('src/m35_phy.c',mk)
        self.assertIn('-Os -flto',mk)
    def test_only_dp_pullup_off(self):
        s=read('src/m37_pullup.c')
        section=s.split('static void disable_only_dp_pullup(void) {',1)[1].split('}',1)[0]
        self.assertIn('AFIO->CTLR &= ~((uint32_t)UDP_PUE_MASK);',section)
        for unexpected in ('USBFS->','GPIOC->','USBPD->','NVIC_','RCC->'):
            self.assertNotIn(unexpected,section)
    def test_restore_original_setting(self):
        s=read('src/m37_pullup.c')
        self.assertIn('saved_dp_pue=AFIO->CTLR & (uint32_t)UDP_PUE_MASK;',s)
        self.assertIn('static void restore_only_dp_pullup(void)',s)
        self.assertIn('| saved_dp_pue;',s)
        self.assertIn("snap(now,'N',true,pe_state);",s)
    def test_full_phy_off_preserves_pd(self):
        s=read('src/m37_pullup.c')
        self.assertIn('USBFS->BASE_CTRL=0;',s)
        self.assertIn('USBFS->UDEV_CTRL=USBFS_UD_PD_DIS;',s)
        self.assertIn('GPIOC->CFGXR & ~0xffu',s)
        self.assertIn('NVIC_DisableIRQ(USB_IRQn)',s)
        self.assertNotIn('USBPD->',s)
    def test_timing_and_restore(self):
        s=read('src/m37_pullup.c')
        for key in ('#define M37_A_MS 30000u','#define M37_B_MS 30000u', '#define M37_C_MS 30000u', '#define M37_RESTORE_MS 500u','(void)USBFSSetup();', 'stage=ST_NO_PULLUP;', 'stage=ST_PULLUP_AGAIN;', 'stage=ST_PHY_OFF;'):
            self.assertIn(key,s)
    def test_integration(self):
        main=read('src/main.c')
        ro=read('src/m5_readonly.c')
        for key in ('m37_pullup_init();','m37_pullup_tick(now, phy_attached()', 'm37_pullup_usb_service_blocked()', 'm37_pullup_led_interval_ms()'):
            self.assertIn(key,main)
        self.assertIn('m37_pullup_reply(body,length,out)',ro)
        self.assertIn('memcmp(body,"RUDIAG",6)',ro)
    def test_script(self):
        s=read('Read-M37-Diag.ps1')
        self.assertIn('RUDIAG',s)
        self.assertIn('ReadLine()',s)
        self.assertIn('$count -gt 9',s)
    def test_mock_runtime(self):
        cc=shutil.which('gcc') or shutil.which('clang')
        if not cc:self.skipTest('No GCC/Clang for mock target')
        with tempfile.TemporaryDirectory() as td:
            exe=Path(td)/('m37_test.exe' if os.name=='nt' else 'm37_test')
            subprocess.run([cc,'-std=c11','-Wall','-Wextra','-Werror','-Itests/m37_stubs','-Isrc',
                            'tests/test_m37_runtime.c','src/m37_pullup.c','-o',str(exe)],cwd=root,check=True,capture_output=True,text=True)
            p=subprocess.run([str(exe)],cwd=root,check=True,capture_output=True,text=True)
            self.assertIn('M37 mocked runtime PASS',p.stdout)
