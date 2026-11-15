from pathlib import Path
import unittest
root=Path(__file__).resolve().parents[1]
main=(root/'src/main.c').read_text(encoding='utf-8')
usb=(root/'src/m28_usb.c').read_text(encoding='utf-8')
fs=(root/'m28_fsusb.c').read_text(encoding='utf-8')
read=(root/'src/m5_readonly.c').read_text(encoding='utf-8')
source=(root/'src/m32_diag.c').read_text(encoding='utf-8')
mk=(root/'Makefile').read_text(encoding='utf-8')
class M32Diag(unittest.TestCase):
 def test_m32_integrated(self):
     assert 'm32_diag_tick(now, phy_attached()' in main
     assert 'm32_diag_usb_irq();' in fs
     assert 'm32_diag_bus_reset_irq();' in usb
     assert 'm32_diag_reply(body,length,out)' in read
     assert 'src/m32_diag.c' in mk
 def test_m32_bounded(self):
     assert '#define M32_N 12u' in source
     assert 'w->len<62u' in source
     assert 'if(n==M32_N) n=M32_N-1u;' in source
     assert 'GPIOC->CFGXR' in source
     assert 'RCC->APB1PCENR' in source
