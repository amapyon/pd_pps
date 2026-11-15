from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[1]

class StructureTests(unittest.TestCase):
    def test_irq_bus_reset_wired(self):
        self.assertIn('m28_usb_on_bus_reset_irq();', (ROOT/'m28_fsusb.c').read_text(encoding='utf-8'))
    def test_two_irq_handlers_are_independent(self):
        self.assertIn('void USBPD_IRQHandler', (ROOT/'src/pd_phy_ch32x035.c').read_text(encoding='utf-8'))
        self.assertIn('void USBFS_IRQHandler', (ROOT/'m28_fsusb.c').read_text(encoding='utf-8'))
    def test_main_runs_pd_before_usb(self):
        main=(ROOT/'src/main.c').read_text(encoding='utf-8')
        self.assertIn('m28_usb_service(now, &policy,',main)
        self.assertLess(main.index('pe_tick(&policy,now);'),main.index('m28_usb_service(now, &policy,'))
        self.assertLess(main.index('pd_stability_tick(&m13c_stats, now,'),main.index('m28_usb_service(now, &policy,'))
        self.assertLess(main.index('m28_usb_service(now, &policy,'),main.index('/* Drain recorded events only after confirmed CC detach + 500 ms.'))
    def test_nonblocking_usb_hold(self):
        usb=(ROOT/'src/m28_usb.c').read_text(encoding='utf-8')
        self.assertNotIn('Delay_Ms(150);',usb)
        self.assertIn('m28_runtime_poll(',usb)
    def test_usb_control_endpoints(self):
        txt=(ROOT/'src/m28_usb.c').read_text(encoding='utf-8')
        self.assertIn('CDC_OUT_EP 2',txt)
        self.assertIn('CDC_IN_EP  3',txt)
        self.assertIn('return -1;',txt)
    def test_live_sensor_is_bounded(self):
        main=(ROOT/'src/main.c').read_text(encoding='utf-8')
        self.assertIn('(uint32_t)(now - m28_sample_ms) <= 2000u',main)
        self.assertIn('pd_sensors_ready_reading()',main)
    def test_pinned_source_was_copied(self):
        self.assertIn('void pe_event(', (ROOT/'src/pd_policy.c').read_text(encoding='utf-8'))
        self.assertIn('void phy_init(', (ROOT/'src/pd_phy_ch32x035.c').read_text(encoding='utf-8'))
    def test_read_only_guard(self):
        s=(ROOT/'src/m5_readonly.c').read_text(encoding='utf-8')
        self.assertIn('read_only',s)
        self.assertNotIn('PE_CMD_REQUEST',s)
    def test_distinct_trial_usb_identity(self):
        txt=(ROOT/'usb_config.h').read_text(encoding='utf-8')
        self.assertIn('FUSB_USB_REV 0x0008',txt)
        self.assertIn('FUSB_STR_SERIAL       u"008"',txt)
    def test_original_pd17_main_has_no_usb_change(self):
        s=(ROOT/'src/main.c').read_text(encoding='utf-8')
        self.assertIn('m28_usb_init();',s)
        self.assertIn('phy_poll_cc(now);',s)

if __name__ == '__main__': unittest.main()
