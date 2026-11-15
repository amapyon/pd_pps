"""Portable Python checks + data consistency with unmodified PD17 and M5-1."""
import copy
import json
import re
import unittest
from pathlib import Path
from pd17_bridge_reference import bridge_pd17, _signed_ma
from src.m5_api import handle_request

ROOT = Path(__file__).resolve().parent.parent
REPOEXP = ROOT.parent
PD17_TEST = REPOEXP / '17_pd_spr_sink_rebuild' / 'tests' / 'test_policy.c'
M51_FIXTURE = REPOEXP / '25_m5_readonly_api' / 'fixtures' / 'pd17_m13g_pps_ready.json'

P = (0x0801912c, 0x0002d12c, 0x0003c12c, 0x0004b12c, 0x0006412c, 0xc1a4213c)

def pe_ready():
    return {'state': 4, 'contract_valid': True, 'pps_active': True,
            'caps_fresh': True, 'caps_epoch': 1, 'caps': {'pdo': list(P), 'count': 6},
            'pdo_index': 6, 'requested_mv': 7000, 'requested_ma': 1000}

def cmd(s, request):
    return handle_request(s, (request + '\n').encode('ascii')).decode('ascii').strip()

class BridgeReferenceTests(unittest.TestCase):
    def test_01_ready_pd17_v7_interface(self):
        b = bridge_pd17(pe_ready())
        self.assertEqual(cmd(b.snapshot, 'GET_CAPS'), 'CAPS status=ok count=6 epoch=1')
        self.assertEqual(cmd(b.snapshot, 'STATUS'), 'STATUS state=ready v=1 pps=1 pos=6 mv=7000 ma=1000')
        self.assertEqual(cmd(b.snapshot, 'GET_CAP 6'), 'CAP i=6 type=pps min=3300 max=21000 ma=3000')
    def test_02_uses_real_pd17_test_constants(self):
        text = PD17_TEST.read_text(encoding='utf-8')
        m = re.search(r'\.pdo\s*=\s*\{([^}]+)\}', text)
        self.assertIsNotNone(m)
        vals = tuple(int(s.strip().rstrip('uU'), 0) for s in m.group(1).split(','))
        self.assertEqual(vals, P)
        fixture = json.loads(M51_FIXTURE.read_text(encoding='utf-8'))['snapshot']
        self.assertEqual(tuple(int(v, 0) for v in fixture['raw_pdos']), P)
    def test_03_detach_hides_cache(self):
        p = pe_ready(); p['state'] = 0
        b = bridge_pd17(p)
        self.assertEqual(cmd(b.snapshot,'GET_CAPS'), 'CAPS status=unavailable count=0 epoch=1')
        self.assertEqual(cmd(b.snapshot,'STATUS'), 'STATUS state=detached v=0 pps=0 pos=0 mv=0 ma=0')
        self.assertEqual(cmd(b.snapshot,'GET_CAP 6'), 'ERR code=caps_unavailable')
    def test_04_recovery_hides_cache(self):
        p = pe_ready(); p['state']=6; p['faults']=19
        b = bridge_pd17(p)
        self.assertEqual(b.faults,19)
        self.assertIn('v=0 pps=0 pos=0 mv=0 ma=0',cmd(b.snapshot,'STATUS'))
        self.assertFalse(b.snapshot.caps_fresh)
    def test_05_renegotiation_does_not_advertise_new_as_contract(self):
        p=pe_ready();p['state']=2
        b=bridge_pd17(p)
        self.assertEqual(cmd(b.snapshot,'STATUS'), 'STATUS state=wait_accept v=0 pps=0 pos=0 mv=0 ma=0')
    def test_06_unfresh_caps_hidden(self):
        p=pe_ready();p['caps_fresh']=False
        b=bridge_pd17(p)
        self.assertFalse(b.snapshot.contract_valid)
        self.assertEqual(cmd(b.snapshot,'GET_CAP 1'),'ERR code=caps_unavailable')
    def test_07_invalid_count_fails_closed(self):
        p=pe_ready();p['caps']['count']=8
        with self.assertRaises(ValueError):bridge_pd17(p)
    def test_08_bad_index_fails_closed(self):
        p=pe_ready();p['pdo_index']=7
        b=bridge_pd17(p)
        self.assertEqual(cmd(b.snapshot,'STATUS'), 'STATUS state=ready v=0 pps=0 pos=0 mv=0 ma=0')
    def test_09_ina_signed_ua_to_whole_ma_and_request_separate(self):
        p=pe_ready();sensor={'ina_ok':True,'ina_bus_mv':6965,'ina_current_ua':-50123,'ina_power_mw':-349,
                             'adc_ok':True,'adc_calibrated_range':True,'adc_cal_mv':7001}
        b=bridge_pd17(p,sensor,sensor_fresh=True)
        self.assertEqual(cmd(b.snapshot,'MEASURE'),'MEASURE status=ok voltage_mV=6965 current_mA=-50')
        self.assertEqual(b.ina_power_mw,-349)
        self.assertEqual(b.adc_cal_mv,7001)
        self.assertIn('ma=1000',cmd(b.snapshot,'STATUS'))
    def test_10_stale_or_failed_sensor_not_numeric(self):
        p=pe_ready();sensor={'ina_ok':True,'ina_bus_mv':5000,'ina_current_ua':1234,'ina_power_mw':4}
        self.assertIn('sensor_unavailable',cmd(bridge_pd17(p,sensor,sensor_fresh=False).snapshot,'MEASURE'))
        sensor['ina_ok']=False
        self.assertIn('sensor_unavailable',cmd(bridge_pd17(p,sensor,sensor_fresh=True).snapshot,'MEASURE'))
    def test_11_input_read_only(self):
        p=pe_ready();unchanged=copy.deepcopy(p)
        s=bridge_pd17(p).snapshot
        self.assertEqual(p,unchanged)
        self.assertEqual(cmd(s,'SET_PPS 7000 1000'),'ERR code=read_only')
    def test_12_state_and_negative_rounding(self):
        for n,expected in [(-1999,-1),(-999,0),(-1000,-1),(0,0),(999,0),(1999,1)]:
            self.assertEqual(_signed_ma(n),expected)
        p=pe_ready();p['state']=100
        with self.assertRaises(ValueError):bridge_pd17(p)

if __name__ == '__main__': unittest.main()
