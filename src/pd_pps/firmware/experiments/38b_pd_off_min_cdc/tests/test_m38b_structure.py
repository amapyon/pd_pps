from pathlib import Path
import re, hashlib, unittest
ROOT = Path(__file__).resolve().parents[1]
class M38BStructure(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.main = (ROOT/'src/main.c').read_text(encoding='utf-8')
        cls.usb  = (ROOT/'src/m38b_usb.c').read_text(encoding='utf-8')
        cls.fs   = (ROOT/'m38b_fsusb.c').read_text(encoding='utf-8')
        cls.make = (ROOT/'Makefile').read_text(encoding='utf-8')
    def test_no_pd_modules_linked(self):
        src=self.make.split('ADDITIONAL_C_FILES :=',1)[1].split('\n',1)[0]
        self.assertEqual(src.strip(),'src/m38b_usb.c m38b_fsusb.c')
        for needle in ('pd_policy.c', 'pd_phy_ch32x035.c', 'pd_sensors.c','pd_vbus_monitor.c','m38_usb.c'):
            self.assertNotIn(needle,src)
    def test_no_pd_runtime_calls_in_main(self):
        clean=re.sub(r'/\*.*?\*/|//[^\n]*','',self.main,flags=re.S)
        for needle in ('phy_init(', 'pe_init(', 'pe_tick(', 'phy_poll_cc(', 'USBPD_IRQn', 'phy_send(', 'pd_sensors_', 'pd_vbus_'):
            self.assertNotIn(needle,clean)
        self.assertIn('m38b_usb_init();',clean)
        self.assertIn('m38b_usb_service();',clean)
    def test_no_pd_runtime_link_in_usb(self):
        self.assertNotIn('pd_policy.h', self.usb)
        self.assertNotIn('USBPD->', self.usb)
        self.assertNotIn('phy_', self.usb)
    def test_irq_counters(self):
        for needle in ('setup_count++','in_count++','out_count++','reset_count++','irq_count++'):
            self.assertIn(needle,self.usb)
        self.assertIn('m38b_usb_on_irq(intfgst);',self.fs)
        self.assertIn('m38b_usb_on_bus_reset_irq();',self.fs)
    def test_no_usb_detach_recovery(self):
        for text in ('usb_detach(', 'usb_attach(', 'USBFSReset(', 'Delay_Ms(150)'):
            self.assertNotIn(text,self.usb)
        self.assertEqual(self.usb.count('USBFSSetup()'),1)
    def test_fs_driver_only_hook_names_changed(self):
        n=self.fs.replace('src/m38b_usb.h','src/m38_usb.h').replace('m38b_usb_on_irq','m38_usb_on_irq').replace('m38b_usb_on_bus_reset_irq','m38_usb_on_bus_reset_irq')
        self.assertEqual(hashlib.sha256(n.encode()).hexdigest(),'83b262bbb935bbd83d5f77e62141b550e5f087b323682dfb599b71e1fdbcd14d')
    def test_usb_identity_distinct(self):
        config=(ROOT/'usb_config.h').read_text(encoding='utf-8')
        self.assertIn('FUSB_USB_REV 0x0039',config)
        self.assertIn('FUSB_STR_SERIAL       u"039"',config)
        self.assertIn('0x1209',config)
        self.assertIn('0xd035',config)
    def test_commands(self):
        for needle in ('"PING"','"STAT"','"COUNT"','PONG M38B','STAT pd=off'):
            self.assertIn(needle,self.usb)
        self.assertNotIn('"DELTA"',self.usb)
    def test_no_pd_irq_from_usb(self):
        a=self.usb.index('void m38b_usb_on_irq')
        b=self.usb.index('void HandleDataOut',a)
        for forbidden in ('printf(', 'phy_', 'pe_', 'USBPD'):
            self.assertNotIn(forbidden,self.usb[a:b])
    def test_memory_options(self):
        self.assertIn('-Os -flto',self.make)
        self.assertIn('CH32X035F8U6',self.make)
    def test_safe_scripts_and_bom(self):
        for name in ('Build-M38B.ps1','Test-M38B-Host.ps1','Read-M38B-Cdc.ps1'):
            s=(ROOT/name).read_bytes()
            self.assertTrue(s.startswith(b'[CmdletBinding()]'),name)
            self.assertFalse(s.startswith(b'\xef\xbb\xbf'),name)
            self.assertIn(b'param(',s)
        for name in ('src/main.c','src/m38b_usb.c','m38b_fsusb.c'):
            s=(ROOT/name).read_bytes()
            self.assertFalse(s.startswith(b'\xef\xbb\xbf'),name)
            self.assertNotEqual(s.splitlines()[0].strip(),b'\\')
    def test_no_false_pd_contract(self):
        self.assertIn('"STAT pd=off pps=0 cfg=%u',self.usb)
        self.assertNotIn('requested_mv',self.usb)
    def test_preserve_usb_31a_fix(self):
        self.assertIn('RCC->APB1PCENR |= RCC_APB1Periph_PWR;',self.fs)
    def test_heartbeat(self):
        self.assertIn('PB12',self.main)
        self.assertIn('>= 250u',self.main)
    def test_no_pd_binaries(self):
        self.assertEqual(list(ROOT.glob('src/pd_*.c')),[])
        self.assertNotIn('USBPD_IRQn',self.main)
if __name__ == '__main__': unittest.main()
