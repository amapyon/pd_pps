"""Pure Python reference for M5-2's PD17 -> M5-1 read-only snapshot.

This module NEVER interfaces with real hardware. The C adapter is the source
for future embedded main-loop integration. This reference validates field
mapping and v7 formatting with M5-1's already-tested Python protocol.
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import sys

M5_1_PATH = Path(__file__).resolve().parent.parent / "25_m5_readonly_api"
if not (M5_1_PATH / "src" / "m5_api.py").exists():
    raise ImportError("M5-1 experiment 25 is missing; install it first")
sys.path.insert(0, str(M5_1_PATH))
from src.m5_api import Sensor, Snapshot, UsbHealth  # noqa: E402

STATE_TO_NAME = (
    "detached", "wait_caps", "wait_accept", "wait_ps_rdy", "ready",
    "soft_reset_wait", "recovery",
)


def _signed_ma(current_ua: int) -> int:
    """Truncate toward zero, mirroring C integer division, no floating point."""
    return current_ua // 1000 if current_ua >= 0 else -((-current_ua) // 1000)


@dataclass(frozen=True)
class BridgeResult:
    snapshot: Snapshot
    faults: int
    ina_power_mw: int | None
    adc_cal_mv: int | None


def bridge_pd17(pe: dict, sensor: dict | None = None, *, sensor_fresh: bool = False,
                usb: UsbHealth = UsbHealth()) -> BridgeResult:
    """Convert one *already-captured*, self-consistent PD17 PE state.

    The caller must synchronize at main-loop level. Never read live pe_t from
    an IRQ or mutate the PE in this adapter. No set commands are supported.
    """
    state = pe.get("state")
    if type(state) is not int or not (0 <= state < len(STATE_TO_NAME)):
        raise ValueError("Invalid PE state")
    name = STATE_TO_NAME[state]
    caps = pe.get("caps", {})
    count = caps.get("count", 0)
    values = caps.get("pdo", [])
    if type(count) is not int or not (0 <= count <= 7) or len(values) < count:
        raise ValueError("Invalid Source Caps count")
    if any(type(v) is not int or not (0 <= v <= 0xFFFFFFFF) for v in values[:count]):
        raise ValueError("Invalid raw PDO")
    visible = name not in ("detached", "recovery") and bool(pe.get("caps_fresh")) and count > 0
    raw = tuple(values[:count]) if visible else ()
    idx = pe.get("pdo_index", 0)
    reqmv = pe.get("requested_mv", 0)
    reqma = pe.get("requested_ma", 0)
    valid = (name == "ready" and bool(pe.get("contract_valid")) and visible
             and type(idx) is int and 1 <= idx <= count and type(reqmv) is int
             and type(reqma) is int and reqmv > 0 and reqma > 0)
    sensor_value = Sensor()
    power = None
    adc = None
    if name == "ready" and sensor_fresh and sensor:
        if sensor.get("ina_ok"):
            sensor_value = Sensor(valid=True,
                                  voltage_mv=int(sensor["ina_bus_mv"]),
                                  current_ma=_signed_ma(int(sensor["ina_current_ua"])))
            power = int(sensor["ina_power_mw"])
        if sensor.get("adc_ok") and sensor.get("adc_calibrated_range"):
            adc = int(sensor["adc_cal_mv"])
    snapshot = Snapshot(
        pd_state=name,
        contract_valid=valid,
        pps_active=bool(pe.get("pps_active")) if valid else False,
        pdo_index=idx if valid else 0,
        requested_mv=reqmv if valid else 0,
        requested_ma=reqma if valid else 0,
        caps_fresh=visible,
        caps_epoch=int(pe.get("caps_epoch", 0)),
        raw_pdos=raw,
        sensor=sensor_value,
        usb=usb,
    )
    return BridgeResult(snapshot=snapshot, faults=int(pe.get("faults", 0)),
                        ina_power_mw=power, adc_cal_mv=adc)
