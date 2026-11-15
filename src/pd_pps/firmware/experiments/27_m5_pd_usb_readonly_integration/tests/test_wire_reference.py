"""Host Python reference regression. Does NOT execute the C integration."""
import sys
import unittest
from pathlib import Path

EXP = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(EXP / '26_m5_pd_snapshot_bridge'))
from pd17_bridge_reference import bridge_pd17, _signed_ma  # noqa: E402
from src.m5_api import handle_request, UsbHealth  # noqa: E402

CAPS = (0x0801912c, 0x0002d12c, 0x0003c12c, 0x0004b12c, 0x0006412c, 0xc1a4213c)

def ready():
    return {'state': 4, 'contract_valid': True, 'pps_active': True,
            'caps_fresh': True, 'caps_epoch': 1,
            'caps': {'pdo': list(CAPS), 'count': 6},
            'pdo_index': 6, 'requested_mv': 7000, 'requested_ma': 1000}

def response(pe, cmd, *, sensor=None, fresh=False):
    snap = bridge_pd17(pe, sensor, sensor_fresh=fresh,
                       usb=UsbHealth(4, 0, 0)).snapshot
    return handle_request(snap, cmd.encode('ascii') + b'\n').decode('ascii')

class ReferenceTests(unittest.TestCase):
    def test_caps_and_v7(self):
        self.assertEqual(response(ready(), 'GET_CAPS'), 'CAPS status=ok count=6 epoch=1\n')
    def test_real_fixed(self):
        self.assertEqual(response(ready(), 'GET_CAP 1'), 'CAP i=1 type=fixed min=5000 max=5000 ma=3000\n')
    def test_real_pps(self):
        self.assertEqual(response(ready(), 'GET_CAP 6'), 'CAP i=6 type=pps min=3300 max=21000 ma=3000\n')
    def test_status_request(self):
        self.assertEqual(response(ready(), 'STATUS'), 'STATUS state=ready v=1 pps=1 pos=6 mv=7000 ma=1000\n')
    def test_current_truncation(self):
        self.assertEqual([_signed_ma(x) for x in [-1999,-999,999,1999]],[-1,0,0,1])
    def test_invalid_measurement(self):
        self.assertEqual(response(ready(), 'MEASURE'), 'MEASURE status=error reason=sensor_unavailable\n')
    def test_detach_invalidates_capabilities(self):
        p=ready();p['state']=0
        self.assertEqual(response(p, 'GET_CAPS'),'CAPS status=unavailable count=0 epoch=1\n')
        self.assertEqual(response(p, 'STATUS'),'STATUS state=detached v=0 pps=0 pos=0 mv=0 ma=0\n')
    def test_read_only(self):
        self.assertEqual(response(ready(), 'SET_PPS 7000 1000'), 'ERR code=read_only\n')
    def test_single_packet(self):
        for cmd in ['GET_CAPS','GET_CAP 6','STATUS','MEASURE','HEALTH']:
            self.assertLessEqual(len(response(ready(),cmd).encode('ascii')),63)

if __name__=='__main__':unittest.main()
