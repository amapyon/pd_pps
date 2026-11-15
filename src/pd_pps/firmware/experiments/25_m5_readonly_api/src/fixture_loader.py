"""Load PD17-derived test fixtures, not live USB/PD hardware."""
from __future__ import annotations
import json
from pathlib import Path
from .m5_api import Sensor, Snapshot, UsbHealth


def load_fixture(path: str | Path) -> Snapshot:
    doc = json.loads(Path(path).read_text(encoding="utf-8"))
    if doc.get("source") != "pd17_host_test_fixture":
        raise ValueError("refusing non-PD17 fixture")
    q = doc["snapshot"]
    raw = tuple(int(x, 0) if isinstance(x, str) else int(x) for x in q["raw_pdos"])
    return Snapshot(
        pd_state=q["pd_state"], contract_valid=q["contract_valid"], pps_active=q["pps_active"],
        pdo_index=q["pdo_index"], requested_mv=q["requested_mv"], requested_ma=q["requested_ma"],
        caps_fresh=q["caps_fresh"], caps_epoch=q["caps_epoch"], raw_pdos=raw,
        sensor=Sensor(**q["sensor"]), usb=UsbHealth(**q["usb"]),
    )
