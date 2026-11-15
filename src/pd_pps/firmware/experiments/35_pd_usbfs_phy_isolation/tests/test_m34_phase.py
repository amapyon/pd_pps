from pathlib import Path
import unittest
root=Path(__file__).resolve().parents[1]
read=lambda p:(root/p).read_text(encoding='utf-8')
class M34(unittest.TestCase):
 def test_hooks(self):
  self.assertIn('m34_diag_usb_irq(intfgst)',read('m28_fsusb.c'))
  self.assertIn('m34_diag_tick(now, phy_attached()',read('src/main.c'))
  self.assertIn('m34_diag_reply(body,length,out)',read('src/m5_readonly.c'))
 def test_phases(self):
  text=read('src/m34_diag.c')
  for fragment in ("snapshot(now,'A'", "snapshot(now,'1'", "snapshot(now,'3'", "snapshot(now,'5'", "snapshot(now,'D'", "snapshot(now,'H'", "snapshot(now,'J'", 'cnt_setup','cnt_reset','CRB_UIF_SUSPEND'):
   self.assertIn(fragment,text)
 def test_readonly_and_bounded(self):
  text=read('src/m34_diag.c')
  self.assertIn('#define M34_SLOTS 8u',text)
  self.assertIn('if(count>=M34_SLOTS)return;',text)
  for forbidden in ('phy_send(', 'Delay_Ms(', 'USBFS->INT_FG =', 'USBFS->BASE_CTRL =', 'pe_event('):
   self.assertNotIn(forbidden,text)
 def test_make(self):
  self.assertIn('-Os -flto',read('Makefile'))
  self.assertIn('src/m34_diag.c',read('Makefile'))
