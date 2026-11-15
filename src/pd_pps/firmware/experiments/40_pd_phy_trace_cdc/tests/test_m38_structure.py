from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
class M38Structure(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.main=(ROOT/'src/main.c').read_text(encoding='utf-8')
        cls.usb=(ROOT/'src/m38_usb.c').read_text(encoding='utf-8')
        cls.fs=(ROOT/'m28_fsusb.c').read_text(encoding='utf-8')
        cls.make=(ROOT/'Makefile').read_text(encoding='utf-8')
    def test_main_preserves_pd_safety_monitor(self):
        for needle in ('pe_init(&policy);','phy_init();','pd_vbus_monitor_tick(now,','phy_abort_for_vbus_loss(',
                       'phy_abort_for_adc_failure()', 'pd_hr_recovery_step(', 'pe_tick(&policy,now);',
                       'phy_send(cmd,policy.rdo,now);'):
            self.assertIn(needle,self.main,needle)
    def test_pd_before_usb(self):
        self.assertLess(self.main.index('pe_tick(&policy,now);'),self.main.index('m38_usb_service(now,'))
        self.assertLess(self.main.index('pd_stability_tick(&m13c_stats, now,'),self.main.index('m38_usb_service(now,'))
    def test_minimal_no_pd_changes(self):
        self.assertIn('src/pd_policy.c src/pd_phy_ch32x035.c',self.make)
        self.assertNotIn('src/m5_',self.make)
        self.assertNotIn('src/m28_runtime.c',self.make)
    def test_no_active_usb_recovery(self):
        for needle in ('usb_detach(', 'usb_attach(', 'USBFSReset(', 'USBFSSetup()'):
            if needle=='USBFSSetup()': self.assertEqual(self.usb.count('USBFSSetup()'),1)
            else: self.assertNotIn(needle,self.usb)
        self.assertNotIn('Delay_Ms(150)',self.usb)
    def test_only_read_only_commands(self):
        for cmd in ('PING','STAT','COUNT','DELTA','PDSTOP','PDSTAB','PDERR','PDTINFO'): self.assertIn(f'"{cmd}"',self.usb)
        self.assertNotIn('PE_CMD_REQUEST',self.usb)
    def test_usb_irq_setup(self):
        self.assertIn('m38_usb_on_irq(intfgst);',self.fs)
        self.assertIn('m38_usb_on_bus_reset_irq();',self.fs)
    def test_no_old_usb_diagnostics(self):
        for text in ('m33_diag_', 'm34_diag_', 'm30_diag_', 'm37_pullup_'):
            self.assertNotIn(text,self.fs)
            self.assertNotIn(text,self.main)
    def test_pd_dedicated_isr(self):
        pd=(ROOT/'src/pd_phy_ch32x035.c').read_text(encoding='utf-8')
        self.assertIn('void USBPD_IRQHandler',pd)
        self.assertIn('void USBFS_IRQHandler',self.fs)
    def test_unique_test_device(self):
        usbc=(ROOT/'usb_config.h').read_text(encoding='utf-8')
        self.assertIn('FUSB_USB_REV 0x0040',usbc)
        self.assertIn('u"040"',usbc)
    def test_read_only_diag_snapshot_wiring(self):
        self.assertIn('&m13c_stats, &m13c2_stop',self.main)
        self.assertIn('PDSTOP active=',self.usb)
        self.assertIn('PDSTAB active=',self.usb)
        self.assertIn('PDERR hr=',self.usb)
        self.assertNotIn('phy_send(',self.usb)
    def test_no_firmware_writes_from_usb_isr(self):
        cb=self.usb[self.usb.index('void m38_usb_on_irq'):self.usb.index('void HandleDataOut')]
        self.assertNotIn('phy_',cb)
        self.assertNotIn('pe_',cb)
        self.assertNotIn('printf(',cb)
    def test_spare_capacity_target(self):
        self.assertIn('-Os -flto',self.make)
    def test_no_unused_m28_sample_cache(self):
        self.assertNotIn('m28_sample_', self.main)
        # PD safety and passive telemetry must stay live.
        for needle in ('pd_sensors_capture_ready();',
                       'pd_sensors_capture_detached();',
                       'pd_sensors_reset_ready();',
                       'sensor_ready_sampled = true;',
                       'pd_vbus_monitor_tick(now,',
                       'phy_abort_for_vbus_loss('):
            self.assertIn(needle, self.main, needle)
    def test_no_py_encoding_issue(self):
        self.assertTrue((ROOT/'src/main.c').read_bytes().startswith(b'#include'))
if __name__ == '__main__': unittest.main()

# SOURCE_GUARD: SHA-256 values of unchanged M37 PD module contents, fixed
# at package creation. Detect accidental edits to safety-critical PD code.
import hashlib
PD_MODULE_SHA256 = {
    'pd_policy.c': 'a0700b80d38c1afa6dec56ebc30a91714bba218e4e4691a9e8af597c1a7e9be3',
    
    'adc_voltage.c': '934b53877359eeafd2905dbb550dc953967c21afb337f8039f5fcd69bd41937a',
    'adc_calibration.c': 'e4d63daef642f776864999aa29343792fd151214f36887771265542cde5a72a9',
    'ina226.c': '9c7407783abb7d9e447d037a10d28f7b5520d3d839d903e42cfc4384aa136846',
    'pd_sensors.c': 'd8c983aa7036cde8d49917ab307d2b856683555d3e70b032ed32f36273326985',
    'pd_vbus_history.c': 'beab0ce020a1604a6b98922d65bf05758d365b6e61cb4694270527a95c2a11d7',
    'pd_vbus_monitor.c': '036e6e5588d1336646a5edff14b8e08dad9878858ad29050b0330b3fce9899e2',
    'pd_hr_vbus.c': '83483a5aef9d3d744b11102d3ad77c354dffa56be3928bf7e707e9fa5e3b9e13',
    'pd_hr_recovery.c': '1753aeff528fa07419dbc29ccd75810c77f4ea8def28b8d063f6e4714806e4d0',
    'pd_stability.c': '0b3889e74a142e3b7ec0c52229061e44be47544adaf87d6922d3a8d8cdc81f38',
    'pd_stop_diag.c': '70b521e6e59aa4041f69d45d5996bfaeb5aa5e45b536862ee92f557cd1d95c40',
}
class M40ReadOnlyTraceSource(unittest.TestCase):
    def test_existing_phy_code_untouched(self):
        phy=(ROOT/'src/pd_phy_ch32x035.c').read_text(encoding='utf-8')
        marker='/* M40: foreground CDC read-only trace snapshot.'
        self.assertEqual(phy.count(marker),1)
        old=phy.split(marker)[0]
        self.assertEqual(hashlib.sha256(old.encode()).hexdigest(),
                         'b7fc1ecd7a9e6ea2cf2a38d463227ab9cb4f9c01ee9f880747473859456287cb')
        addition=phy.split(marker)[1]
        self.assertNotIn('NVIC_DisableIRQ',addition)
        self.assertNotIn('trace_tail =',addition)
        self.assertNotIn('USBPD->',addition)
    def test_peek_is_only_used_from_foreground(self):
        usb=(ROOT/'src/m38_usb.c').read_text(encoding='utf-8')
        self.assertIn('phy_trace_snapshot(', usb)
        self.assertIn('PDTRACE idx=', usb)
        self.assertIn('PDTINFO count=', usb)
        cb=usb[usb.index('void m38_usb_on_irq'):usb.index('void HandleDataOut')]
        self.assertNotIn('phy_trace_snapshot(',cb)

class M38PinnedPDSource(unittest.TestCase):
    def test_pd_modules_match_m37(self):
        for filename, sha in PD_MODULE_SHA256.items():
            got = hashlib.sha256((ROOT/'src'/filename).read_bytes()).hexdigest()
            self.assertEqual(got, sha, filename)
