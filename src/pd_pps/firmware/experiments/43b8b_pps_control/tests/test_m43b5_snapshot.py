from pathlib import Path
import hashlib
import importlib.util
import re
import struct
import unittest

ROOT=Path(__file__).resolve().parents[1]
EXPECTED_BASE='ec85175aaf61a5f00901b966ffe18501c9b171f04497a6adf8b7be0b4f5460f8'

class M43B5RetainedB4Hooks(unittest.TestCase):
    def test_phy_is_b3_with_diagnostic_blocks_only(self):
        from test_m43b5_baseline import restore_b4_phy
        body=restore_b4_phy((ROOT/'src/pd_phy_ch32x035.c').read_text(encoding='utf-8'))
        labels=['TX_LAUNCH','RX_IRQ','RX_ARM_REG','CRC_TIMEOUT']
        for label in labels:
            pattern=r'    /\* M43B4 SNAP BEGIN: '+label+r' \*/\n.*?    /\* M43B4 SNAP END: '+label+r' \*/\n'
            body,n=re.subn(pattern,'',body,flags=re.S)
            self.assertEqual(n,1,label)
        self.assertEqual(hashlib.sha256(body.encode('utf-8')).hexdigest(),EXPECTED_BASE)
    def test_event_schema_unchanged_except_codes(self):
        hdr=(ROOT/'src/m43b3_sram.h').read_text(encoding='utf-8')
        self.assertIn('#define M43B3_SRAM_VERSION 1u',hdr)
        self.assertIn('#define M43B3_SRAM_CAPACITY 64u',hdr)
        for code in range(10,15):
            self.assertRegex(hdr,r'M43B4_EVT_[A-Z_]+ = '+str(code)+r'u')
        self.assertIn('m43b3_sram', (ROOT/'Read-M43B7-SRAM.ps1').read_text(encoding='utf-8'))
    def test_rx_sampling_precedes_packet_handler(self):
        x=(ROOT/'src/pd_phy_ch32x035.c').read_text(encoding='utf-8')
        i=x.index('    if (status & IF_RX_ACT) {')
        part=x[i:x.index('        USBPD->STATUS = IF_RX_ACT;',i)]
        self.assertLess(part.index('M43B4_EVT_RX_IRQ'),part.index(') receive_packet();'))
        self.assertIn('m43b4_n <= sizeof(dma_buffer)',part)
        self.assertIn('tx_wait_crc && crc_wait_armed',part)
        self.assertNotIn('printf(',part)
    def test_decoder_new_events(self):
        spec=importlib.util.spec_from_file_location('b4dec',ROOT/'Decode-M43B5-SRAM.py')
        mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
        raw=bytearray(mod.RAM_BYTES);off=0x200
        struct.pack_into('<16I',raw,off,mod.MAGIC,1,64,16,64,1,5,5,5,1,800,0,0,1,0,mod.END_MAGIC)
        entries=[(10,0,0x1a060082,0x00060098),(11,7,0x01160021,0x000641a1),
                 (12,0,0x11223344,0x02007abc),(13,3,0x01060306,0x02000098),
                 (14,3,0x01060018,0x00020006)]
        for i,(code,flag,a0,a1) in enumerate(entries):
            struct.pack_into('<IIIHBB',raw,off+64+i*16,100+i,a0,a1,i,code,flag)
        d,rows=mod.decode(bytes(raw),mod.RAM_BASE+off)
        self.assertEqual([r['event'] for r in rows],
            ['PHY_TX_LAUNCH','PHY_RX_IRQ','PHY_RX_SAMPLE','PHY_RX_ARM_REG','PHY_CRC_TIMEOUT'])
        self.assertIn('dma_0_5=44332211bc7a',rows[2]['decode'])
        self.assertIn('sop=1',rows[1]['decode'])
    def test_makefile_new_target_and_no_replacement_of_b3_storage(self):
        mk=(ROOT/'Makefile').read_text(encoding='utf-8')
        self.assertIn('TARGET := m43b8b_pps_control',mk)
        self.assertIn('src/m43b3_sram.c',mk)
        self.assertNotIn('src/m43b4_sram.c',mk)

if __name__=='__main__':unittest.main()
