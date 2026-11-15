from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[1]
class M31AClock(unittest.TestCase):
    def test_apb1_pwr_clock_enabled(self):
        s=(ROOT/'m28_fsusb.c').read_text(encoding='utf-8')
        self.assertIn('RCC->APB1PCENR |= RCC_APB1Periph_PWR;',s)
        self.assertNotIn('RCC_APB2Periph_GPIOC | RCC_APB1Periph_PWR',s)
    def test_pvd_based_phy_choice_preserved(self):
        s=(ROOT/'m28_fsusb.c').read_text(encoding='utf-8')
        self.assertIn('PWR->CSR & PWR_CSR_PVDO',s)
        self.assertIn('USB_PHY_V33 | UDP_PUE_1K5 | USB_IOEN',s)
        self.assertIn('UDP_PUE_10K | USB_IOEN',s)
    def test_led_and_diag_retained(self):
        s=(ROOT/'src/main.c').read_text(encoding='utf-8')
        self.assertIn('funDigitalWrite(PB12',s)
        self.assertIn('m30_diag_tick',s)
if __name__=='__main__': unittest.main()
