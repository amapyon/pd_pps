from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class M43B2IrqPriority(unittest.TestCase):
    def test_priority_values_and_order(self):
        main = (ROOT / 'src/main.c').read_text(encoding='utf-8')
        pd = 'NVIC_SetPriority(USBPD_IRQn, 0x00u);'
        usb = 'NVIC_SetPriority(USBFS_IRQn, 0x80u);'
        self.assertEqual(main.count(pd), 1)
        self.assertEqual(main.count(usb), 1)
        self.assertIn('phy_init();', (ROOT / 'src/pd_session.c').read_text(encoding='utf-8'))
        self.assertLess(main.index('pd_session_init_core();'), main.index(pd))
        self.assertLess(main.index(pd), main.index(usb))
        self.assertLess(main.index(usb), main.index('m38_usb_init();'))
    def test_ch32x035_irq_symbols(self):
        # The MCU header is supplied by the existing ch32fun submodule.
        header = ROOT.parents[2] / 'ch32fun/ch32fun/ch32x03xhw.h'
        if not header.is_file():
            self.skipTest(f'ch32fun header is supplied by installed project: {header}')
        hw = header.read_text(encoding='utf-8')
        self.assertRegex(hw, r'USBFS_IRQn\s*=\s*45\s*,')
        self.assertRegex(hw, r'USBPD_IRQn\s*=\s*49\s*,')
        main = (ROOT / 'src/main.c').read_text(encoding='utf-8')
        self.assertNotIn('NVIC_SetPriority(USB_IRQn,', main)
    def test_source_isolated_from_M43A(self):
        wrapper = (ROOT / 'm43b7_p2_session_refactor.c').read_text(encoding='utf-8')
        mk = (ROOT / 'Makefile').read_text(encoding='utf-8')
        self.assertIn('#include "src/main.c"', wrapper)
        self.assertIn('TARGET := m43b7_p2_session_refactor', mk)
    def test_no_other_priority_changes_in_interrupt_handlers(self):
        phy = (ROOT / 'src/pd_phy_ch32x035.c').read_text(encoding='utf-8')
        usb = (ROOT / 'm28_fsusb.c').read_text(encoding='utf-8')
        self.assertNotIn('NVIC_SetPriority(', phy)
        self.assertNotIn('NVIC_SetPriority(', usb)

if __name__ == '__main__':
    unittest.main()
