from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
class M30Diag(unittest.TestCase):
    def test_diagnostic_commands_and_capture(self):
        s=(ROOT/'src/m30_diag.c').read_text(encoding='utf-8')
        self.assertIn('m30_diag_reply',s)
        self.assertIn('USBFS->BASE_CTRL',s)
        self.assertIn('USBFS->UDEV_CTRL',s)
        self.assertIn('USBFSCTX.USBFS_DevConfig',s)
        self.assertIn('AFIO->CTLR',s)
    def test_main_priority(self):
        s=(ROOT/'src/main.c').read_text(encoding='utf-8')
        self.assertLess(s.index('phy_poll_cc(now)'),s.index('m30_diag_tick(now'))
        self.assertLess(s.index('m30_diag_tick(now'),s.index('m28_usb_service(now'))
    def test_usb_irq_counter(self):
        self.assertIn('m30_diag_usb_irq();',(ROOT/'m28_fsusb.c').read_text(encoding='utf-8'))
        self.assertIn('m30_diag_bus_reset_irq();',(ROOT/'src/m28_usb.c').read_text(encoding='utf-8'))
if __name__=='__main__': unittest.main()
