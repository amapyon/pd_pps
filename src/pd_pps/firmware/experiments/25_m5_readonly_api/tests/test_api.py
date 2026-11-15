"""Pure-host acceptance tests for protocol v7 DRAFT.
Run with: python -m unittest discover -s tests -v
"""
from __future__ import annotations
import unittest
from pathlib import Path
from src.fixture_loader import load_fixture
from src.m5_api import MAX_LINE, Sensor, Snapshot, UsbHealth, handle_request

ROOT = Path(__file__).resolve().parent.parent


def fixture(name: str):
    return load_fixture(ROOT / "fixtures" / name)


def run(snap, cmd: str) -> str:
    return handle_request(snap, cmd.encode("ascii") + b"\n").decode("ascii").strip()


class ApiTests(unittest.TestCase):
    def setUp(self):
        self.ready = fixture("pd17_m13g_pps_ready.json")
        self.detached = fixture("detached.json")

    def test_info_identification_is_marked_mock(self):
        self.assertEqual(run(self.ready, "INFO"), "INFO board=CH32X035F8U6 firmware=m5_1_mock protocol=7")

    def test_health_counters(self):
        self.assertEqual(run(self.detached, "HEALTH"), "HEALTH bus_resets=12 tx_timeouts=0 reconnects=0")

    def test_source_caps_counts(self):
        self.assertEqual(run(self.ready, "GET_CAPS"), "CAPS status=ok count=6 epoch=1")

    def test_real_pd17_fixed_pdo(self):
        self.assertEqual(run(self.ready, "GET_CAP 1"), "CAP i=1 type=fixed min=5000 max=5000 ma=3000")

    def test_real_pd17_pps_apdo(self):
        self.assertEqual(run(self.ready, "GET_CAP 6"), "CAP i=6 type=pps min=3300 max=21000 ma=3000")

    def test_ready_contract_requested_not_measured_current(self):
        self.assertEqual(run(self.ready, "STATUS"), "STATUS state=ready v=1 pps=1 pos=6 mv=7000 ma=1000")
        self.assertEqual(run(self.ready, "MEASURE"), "MEASURE status=error reason=sensor_unavailable")

    def test_fixed_only(self):
        fixed = fixture("fixed_only.json")
        self.assertEqual(run(fixed, "STATUS"), "STATUS state=ready v=1 pps=0 pos=1 mv=5000 ma=1000")
        self.assertEqual(run(fixed, "GET_CAPS"), "CAPS status=ok count=1 epoch=3")

    def test_disconnected_must_hide_stale_caps_and_contract(self):
        self.assertEqual(run(self.detached, "GET_CAPS"), "CAPS status=unavailable count=0 epoch=2")
        self.assertEqual(run(self.detached, "GET_CAP 1"), "ERR code=caps_unavailable")
        self.assertEqual(run(self.detached, "STATUS"), "STATUS state=detached v=0 pps=0 pos=0 mv=0 ma=0")

    def test_missing_sensor_never_returns_stale_values(self):
        self.assertNotIn("voltage_mV=",run(self.detached,"MEASURE"))
        self.assertEqual(run(self.detached, "MEASURE"), "MEASURE status=error reason=sensor_unavailable")

    def test_sensor_values_when_valid(self):
        d = self.ready.__dict__.copy()
        d["sensor"] = Sensor(valid=True, voltage_mv=7000, current_ma=-50)
        got = run(Snapshot(**d), "MEASURE")
        self.assertEqual(got, "MEASURE status=ok voltage_mV=7000 current_mA=-50")

    def test_out_of_range_and_malformed_index(self):
        for s in ["GET_CAP 0", "GET_CAP 01", "GET_CAP -1", "GET_CAP +1", "GET_CAP foo", "GET_CAP 999"]:
            with self.subTest(command=s):
                self.assertTrue(run(self.ready,s).startswith("ERR code="))
        self.assertEqual(run(self.ready, "GET_CAP 7"), "ERR code=out_of_range")

    def test_only_read_commands_allowed(self):
        for s in ("SET_PPS 7000 1000", "SET_OUTPUT_ON", "OUTPUT_ON"):
            self.assertEqual(run(self.ready, s), "ERR code=read_only")
        self.assertEqual(run(self.ready, "FLASH"), "ERR code=unknown_command")

    def test_line_framing(self):
        for req in (b"", b"INFO", b"INFO\nINFO\n", b"INFO\rX\n", b"INFO\x00\n",b"\xff\n", b"INFO\r\r\n", b"A" * 64 + b"\n"):
            with self.subTest(request=req):
                self.assertEqual(handle_request(self.ready, req), b"ERR code=bad_line\n")
        self.assertEqual(handle_request(self.ready, b"INFO\r\n"), handle_request(self.ready, b"INFO\n"))

    def test_response_max_size_including_newline(self):
        command_set = [b"INFO\n", b"HEALTH\n", b"GET_CAPS\n", b"STATUS\n", b"MEASURE\n"]
        command_set += [f"GET_CAP {i}\n".encode() for i in range(1,8)]
        for snap in (self.ready,self.detached, fixture("fixed_only.json")):
            for cmd in command_set:
                with self.subTest(state=snap.pd_state, command=cmd):
                    out=handle_request(snap,cmd)
                    self.assertLessEqual(len(out),MAX_LINE)
                    self.assertTrue(out.endswith(b"\n"))
                    self.assertEqual(out.count(b"\n"),1)

    def test_unknown_pdo_fails_closed_without_bad_decoding(self):
        d=self.ready.__dict__.copy();d["raw_pdos"]=(0x40000000,0x80000000,0xF0000000)
        d["contract_valid"]=False; d["pps_active"]=False;d["caps_fresh"]=True
        d["pdo_index"]=0
        snap=Snapshot(**d)
        for i in (1,2,3):
            self.assertIn("type=unsupported raw=0x",run(snap,f"GET_CAP {i}"))

    def test_state_validation(self):
        d=self.ready.__dict__.copy();d["raw_pdos"] = tuple(range(8))
        with self.assertRaises(ValueError): Snapshot(**d)
        d=self.ready.__dict__.copy();d["pps_active"]=False;d["contract_valid"]=False
        self.assertEqual(run(Snapshot(**d),"STATUS"),"STATUS state=ready v=0 pps=0 pos=0 mv=0 ma=0")

    def test_maximum_status_and_health_does_not_overflow(self):
        d=self.ready.__dict__.copy()
        d["pd_state"]="soft_reset_wait"; d["requested_mv"]=99999; d["requested_ma"]=99999
        out=handle_request(Snapshot(**d),b"STATUS\n")
        self.assertTrue(out.startswith(b"STATUS state=soft_reset_wait"))
        self.assertLessEqual(len(out),MAX_LINE)
        d=self.ready.__dict__.copy();d["usb"]=UsbHealth(9999999,9999999,9999999)
        self.assertEqual(run(Snapshot(**d),"HEALTH"),"ERR code=reply_too_long")

    def test_simulated_reconnect_requires_fresh_caps(self):
        before=run(self.ready,"GET_CAPS")
        after_disconnection=run(self.detached,"GET_CAPS")
        after_reconnect=run(self.ready,"GET_CAPS")
        self.assertNotEqual(before,after_disconnection)
        self.assertEqual(before,after_reconnect)


if __name__ == "__main__": unittest.main()
