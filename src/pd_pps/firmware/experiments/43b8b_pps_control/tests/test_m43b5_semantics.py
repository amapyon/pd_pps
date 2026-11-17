from pathlib import Path
import importlib.util
import hashlib
import struct
import unittest
ROOT=Path(__file__).resolve().parents[1]

def read(path):return (ROOT/path).read_text(encoding='utf-8')
class M43B5Semantics(unittest.TestCase):
    def test_policy_algorithm_unchanged(self):
        # Preserve the *original* M43B-5 policy as a pinned behavior oracle.
        self.assertEqual(hashlib.sha256((ROOT/'tests/fixtures/pd_policy_m43b5_v01.c').read_bytes()).hexdigest(), 'a0700b80d38c1afa6dec56ebc30a91714bba218e4e4691a9e8af597c1a7e9be3')
        self.assertEqual(hashlib.sha256((ROOT/'m28_fsusb.c').read_bytes()).hexdigest(), '7f7fac2e2ef64d5c9e01a03d02195a754b11e067d3abb6deb8ffc7c81262f3e9')
    def test_source_hr_freezes_only_on_second_event(self):
        src=read('src/pd_phy_ch32x035.c')
        hr=src[src.index('if (status & IF_RX_RESET) {'):src.index('hard_reset = true;',src.index('if (status & IF_RX_RESET) {'))]
        self.assertIn('m43b3_sram.reserved1 = m43b5_hr_count;',hr)
        self.assertIn('if (m43b5_hr_count >= 2u)',hr)
        self.assertNotIn('m43b3_sram_freeze(M43B3_FREEZE_SOURCE_HR, SysTick->CNTL); /* M43B3 SRAM */',hr)
        self.assertLess(hr.index('m43b5_finish_request(3u)'),hr.index('m43b3_sram_freeze(M43B3_FREEZE_SOURCE_HR'))
    def test_per_request_crc_window_atomic_snapshot(self):
        phy=read('src/pd_phy_ch32x035.c')
        diag=read('src/m43b2_diag.c')
        self.assertIn('m43b5_window_irq_base = m43b2_diag_usbfs_crc_count();',phy)
        self.assertIn('m43b2_diag_usbfs_crc_count() - m43b5_window_irq_base',phy)
        self.assertIn('if (command == PE_CMD_REQUEST) m43b5_begin_request(h);',phy)
        self.assertIn('m43b5_window_finish(2u)',phy)
        self.assertIn('m43b5_finish_request(2u)',phy)
        self.assertIn('return d.usbfs_crc_window',diag)
    def test_request_phase_marked_by_existing_policy_transition(self):
        main=read('src/pd_session.c')
        self.assertIn('if (was_pps_ready && policy.state == PE_WAIT_ACCEPT) {',main)
        self.assertIn('phy_m43b5_mark_refresh();',main)
    def test_binary_abi(self):
        hdr=read('src/m43b3_sram.h')
        self.assertIn('#define M43B3_SRAM_VERSION 1u',hdr)
        self.assertIn('#define M43B3_SRAM_CAPACITY 64u',hdr)
        for code in range(15,19):
            self.assertRegex(hdr,r'M43B5_EVT_[A-Z_]+ = '+str(code)+r'u')
    def test_decoder_events_15_to_18(self):
        spec=importlib.util.spec_from_file_location('b5',ROOT/'Decode-M43B5-SRAM.py')
        mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
        ram=bytearray(mod.RAM_BYTES); off=0x180
        struct.pack_into('<16I',ram,off,mod.MAGIC,1,64,16,64,1,5,5,5,1,1000,0,0,25,1,mod.END_MAGIC)
        vals=[(15,2,1|(0x1082<<16),90),(16,2,1|(0<<16),3),
              (16,2,1|(1<<16),2),(17,2,1|(1<<16),5),(18,1,0x5a,6)]
        for i,(code,flag,a0,a1) in enumerate(vals):
            struct.pack_into('<IIIHBB',ram,off+64+16*i,100+i,a0,a1,i,code,flag)
        d,rows=mod.decode(ram,mod.RAM_BASE+off)
        self.assertEqual(d['reserved1'],1)
        self.assertIn('PPS_REFRESH',rows[0]['decode'])
        self.assertIn('fs_crc_irq=3',rows[1]['decode'])
        self.assertIn('total_fs_crc_irq=5',rows[3]['decode'])
        self.assertIn('source_hr_ordinal=1',rows[4]['decode'])
    def test_old_sram_resident_layout_not_changed(self):
        s=read('src/m43b3_sram.c')
        self.assertIn('sizeof(m43b3_sram_t)==1088u',s)
        self.assertIn('m43b3_sram.reserved1 = 0u;',s)
    def test_flash_is_not_in_installer(self):
        script=Path(__file__).resolve().parents[4]/'install.ps1'
        if not script.exists():self.skipTest('Installer outside extracted project')
        text=script.read_text(encoding='utf-8')
        self.assertNotIn('minichlink.exe',text)
        self.assertNotIn("'FLASH'",text)
