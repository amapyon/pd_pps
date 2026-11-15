from pathlib import Path
import unittest
root=Path(__file__).resolve().parents[1]
read=lambda p:(root/p).read_text(encoding='utf-8')
class M33Trace(unittest.TestCase):
 def test_hooks(self):
  self.assertIn('m33_diag_usb_irq(intfgst)',read('m28_fsusb.c'))
  self.assertIn('m33_diag_set_config_irq(ctx->USBFS_DevConfig)',read('m28_fsusb.c'))
  self.assertIn('m33_diag_tick(now, phy_attached()',read('src/main.c'))
  self.assertIn('m33_diag_reply(body,length,out)',read('src/m5_readonly.c'))
 def test_bound_and_gate(self):
  text=read('src/m33_diag.c')
  for token in ('M33_EVENTS 40u','if (!logging || used>=M33_EVENTS)','++used;','logging=0;','session_setup','session_reset','CUIS_TOKEN_SETUP','CRB_UIF_BUS_RST'):
   self.assertIn(token,text)
 def test_makefile(self):
  self.assertIn('src/m33_diag.c',read('Makefile'))
  self.assertIn('TARGET := m36_pd_usbfs_pullup_isolation',read('Makefile'))
  self.assertIn('#include "src/main.c"',read('m36_pd_usbfs_pullup_isolation.c'))
 def test_no_pd_write(self):
  text=read('src/m33_diag.c')
  for token in ('phy_send','pe_event','Delay_Ms','USBFS->INT_FG ='):
   self.assertNotIn(token,text)
