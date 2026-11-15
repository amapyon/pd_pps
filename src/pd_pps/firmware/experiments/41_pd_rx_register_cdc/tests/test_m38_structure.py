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
        self.assertIn('FUSB_USB_REV 0x0041',usbc)
        self.assertIn('u"041"',usbc)
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
class M41RxTraceSource(unittest.TestCase):
    def test_observational_register_snapshots(self):
        phy=(ROOT/'src/pd_phy_ch32x035.c').read_text(encoding='utf-8')
        header=(ROOT/'src/pd_phy_ch32x035.h').read_text(encoding='utf-8')
        for kind in ('IRQ_REG','RX_DMA','RX_ARM','CRC_TIMEOUT','CRC_CTX','RX_GATED'):
            self.assertIn('PDTR_M41_'+kind,phy)
            self.assertIn('PDTR_M41_'+kind,header)
        self.assertIn('const uint16_t rx_bytes',phy)
        self.assertIn('if (rx_bytes >= 2u && rx_bytes <= sizeof(dma_buffer))',phy)
        self.assertIn('trace_add(PDTR_M41_RX_DMA',phy)
        self.assertIn('m41_control_config()',phy)
        self.assertIn('void USBPD_IRQHandler(void)',phy)
    def test_existing_pd_tx_and_recovery_functions_identical(self):
        # Extract and compare unmodified critical function bodies to M40
        import zipfile
        # Keep this test standalone: compare against pinned per-function digests.
        phy=(ROOT/'src/pd_phy_ch32x035.c').read_text(encoding='utf-8')
        def body(name):
            import re
            hit=re.search(r'(?m)^(?:static )?(?:void|bool|uint32_t|uint16_t)\s+'+name+r'\([^;]*?\)\s*\{',phy)
            self.assertIsNotNone(hit,name)
            pos=hit.end()-1
            depth=0
            for i in range(pos,len(phy)):
                if phy[i]=='{': depth+=1
                if phy[i]=='}':
                    depth-=1
                    if depth==0: return phy[hit.start():i+1]
            self.fail('Unbalanced braces '+name)
        EXPECT={
            'phy_send': '956001096a838bd200b019815e734690703fc654d9c707d148145173ec452584',
            'good_crc': 'ef5ff0aca81d99bed1df68f392c5cf91767470143001b8e4c8808ecb9a068b0d',
            'transmit': '2ba6088c2418e8967729a1de41c5415501a7b527ada8fdb4509abdb82890f035',
            'rx_mode': '9a71ec8cc8db81569d7c81fe9749ae9fb71748bb8bf44bb01adf3357d0637e19',
            'phy_init': '2a2fcbf9edca5ad015e7a5c4830245f3952dcc17b06b434d096a40a0ed484af4',
            'phy_poll_cc': '5884f6a81f695b438bcb06fc0acf327d93930ce6f438f3d05073c6543c809016',
            'phy_hr_arm_rx': '1ab2c2a467b7da295b93db8077e6ba104375b297b2fa89d5c6ec2bda00976f97',
            'phy_hr_complete_recovery': '1471934fa54d68afaa26bc210d442b3af4d5a7fee06e9024c2535ef8501edc49',
            'fail_closed_for_fault': '81e88a3821081ca5b075d8b14e81d82c67aa016e959b3bc169cacfbc001616a3',
        }
        # Presence and original byte-level names guard accidental loss.
        for name in ('phy_send','good_crc','transmit','rx_mode',
                     'phy_init','phy_poll_cc','phy_hr_arm_rx',
                     'phy_hr_complete_recovery','fail_closed_for_fault'):
            self.assertEqual(hashlib.sha256(body(name).encode()).hexdigest(), EXPECT[name], name)
    def test_trace_not_read_from_usb_isr(self):
        usb=(ROOT/'src/m38_usb.c').read_text(encoding='utf-8')
        cb=usb[usb.index('void m38_usb_on_irq'):usb.index('void HandleDataOut')]
        self.assertNotIn('phy_trace_snapshot(',cb)
        self.assertIn('PDTRACE idx=',usb)
        self.assertIn('PDTINFO count=',usb)

class M38PinnedPDSource(unittest.TestCase):
    def test_pd_modules_match_m37(self):
        for filename, sha in PD_MODULE_SHA256.items():
            got = hashlib.sha256((ROOT/'src'/filename).read_bytes()).hexdigest()
            self.assertEqual(got, sha, filename)
