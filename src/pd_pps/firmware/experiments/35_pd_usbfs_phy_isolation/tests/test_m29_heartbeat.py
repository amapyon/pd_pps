from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[1]
class M29Heartbeat(unittest.TestCase):
    def test_gpio_config_and_timing(self):
        s=(ROOT/'src/main.c').read_text(encoding="utf-8")
        self.assertIn('funPinMode(PB12, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP)',s)
        self.assertIn('funDigitalWrite(PB12, FUN_LOW)',s)
        self.assertIn('m35_phy_is_off() ? 100u : 250u',s)
        self.assertIn('funDigitalWrite(PB12, m29_led_on ? FUN_HIGH : FUN_LOW)',s)
    def test_pd_prioritized_after_led(self):
        s=(ROOT/'src/main.c').read_text(encoding="utf-8")
        self.assertLess(s.index('phy_poll_cc(now)'),s.index('m28_usb_service(now, &policy,'))
        self.assertIn('phy_maintain(now)',s)
if __name__ == '__main__': unittest.main()
