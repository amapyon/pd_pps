"""Host-only read-only USB CDC ASCII API design prototype.

No physical USB, Power Delivery or INA226 operations are performed here.
The input state comes from deliberately labelled PD17-based test fixtures.
This module is intended as a pure formatting/validation reference for
an eventual embedded implementation, NOT a production firmware or source driver.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

MAX_LINE = 63  # including LF; conservative one-packet application limit
MAX_CAPS = 7   # bound taken from pd17 src/pd_policy.h, pe_caps_t.pdo[7]
VALID_STATES = frozenset(("detached", "wait_caps", "wait_accept", "wait_ps_rdy", "ready", "soft_reset_wait", "recovery"))


@dataclass(frozen=True)
class Sensor:
    valid: bool = False
    voltage_mv: Optional[int] = None
    current_ma: Optional[int] = None


@dataclass(frozen=True)
class UsbHealth:
    bus_resets: int = 0
    tx_timeouts: int = 0
    reconnects: int = 0


@dataclass(frozen=True)
class Snapshot:
    # Snapshot must come from one consistent policy state; sample fixture only.
    pd_state: str
    contract_valid: bool
    pps_active: bool
    pdo_index: int
    requested_mv: int
    requested_ma: int
    caps_fresh: bool
    caps_epoch: int
    raw_pdos: tuple[int, ...]
    sensor: Sensor = Sensor()
    usb: UsbHealth = UsbHealth()

    def __post_init__(self) -> None:
        if self.pd_state not in VALID_STATES:
            raise ValueError("unexpected PE state")
        if len(self.raw_pdos) > MAX_CAPS:
            raise ValueError("too many Source PDOs")
        if any(not (0 <= x <= 0xFFFFFFFF) for x in self.raw_pdos):
            raise ValueError("invalid raw PDO")
        if not (0 <= self.caps_epoch <= 255):
            raise ValueError("invalid caps epoch")
        if self.contract_valid and not (1 <= self.pdo_index <= len(self.raw_pdos)):
            raise ValueError("contract has no indexed PDO")
        if self.pps_active and not self.contract_valid:
            raise ValueError("PPS cannot be active without contract")
        if self.contract_valid and (self.requested_mv <= 0 or self.requested_ma <= 0):
            raise ValueError("invalid contracted request")
        if self.sensor.valid and (self.sensor.voltage_mv is None or self.sensor.current_ma is None):
            raise ValueError("valid sensor needs both readings")
        if self.sensor.valid and (not (0 <= self.sensor.voltage_mv <= 99999)
                                  or not (-99999 <= self.sensor.current_ma <= 99999)):
            raise ValueError("sensor outside mock format bound")
        if any(x < 0 or x > 9999999 for x in (self.usb.bus_resets, self.usb.tx_timeouts, self.usb.reconnects)):
            raise ValueError("invalid USB counters")


def _line(body: str) -> bytes:
    out = (body + "\n").encode("ascii")
    if len(out) > MAX_LINE:
        return b"ERR code=reply_too_long\n"
    return out


def _decode_cap(index: int, raw: int) -> str:
    # PD17's real pe_caps_t retains 32-bit raw Source_Capabilities.
    typ = (raw >> 30) & 3
    if typ == 0:  # Fixed Supply PDO: 50 mV & 10 mA units
        voltage_mv = ((raw >> 10) & 0x3FF) * 50
        current_ma = (raw & 0x3FF) * 10
        if voltage_mv and current_ma:
            return f"CAP i={index} type=fixed min={voltage_mv} max={voltage_mv} ma={current_ma}"
    if typ == 3 and ((raw >> 28) & 3) == 0:  # SPR PPS APDO: 100mV/50mA units
        max_mv = ((raw >> 17) & 0xFF) * 100
        min_mv = ((raw >> 8) & 0xFF) * 100
        current_ma = (raw & 0x7F) * 50
        if min_mv and min_mv <= max_mv and current_ma:
            return f"CAP i={index} type=pps min={min_mv} max={max_mv} ma={current_ma}"
    # Do not pretend to understand Battery/Variable/EPR or malformed PDOs.
    return f"CAP i={index} type=unsupported raw=0x{raw:08X}"


def _validate_request(request: bytes) -> str | None:
    if not isinstance(request, bytes):
        return None
    if len(request) > MAX_LINE or len(request) == 0:
        return None
    # Every request must be exactly one ASCII line, terminated by LF.
    if not request.endswith(b"\n") or request.count(b"\n") != 1:
        return None
    body = request[:-1]
    if body.endswith(b"\r"):
        body = body[:-1]
    if not body or b"\r" in body or any(c < 32 or c > 126 for c in body):
        return None
    try:
        return body.decode("ascii")
    except UnicodeDecodeError:
        return None


def handle_request(snapshot: Snapshot, request: bytes) -> bytes:
    """One request yields exactly one ASCII LF-terminated, <=63-byte reply.

    All functions are read-only: no USB reopen, PE transition, PPS Request,
    sensor sampling, or persistent writes can be triggered by this code.
    """
    req = _validate_request(request)
    if req is None:
        return _line("ERR code=bad_line")
    if req == "INFO":
        return _line("INFO board=CH32X035F8U6 firmware=m5_1_mock protocol=7")
    if req == "HEALTH":
        h = snapshot.usb
        return _line(f"HEALTH bus_resets={h.bus_resets} tx_timeouts={h.tx_timeouts} reconnects={h.reconnects}")
    if req == "GET_CAPS":
        if not snapshot.caps_fresh or snapshot.pd_state in ("detached", "recovery"):
            return _line(f"CAPS status=unavailable count=0 epoch={snapshot.caps_epoch}")
        return _line(f"CAPS status=ok count={len(snapshot.raw_pdos)} epoch={snapshot.caps_epoch}")
    if req.startswith("GET_CAP "):
        n = req[len("GET_CAP "):]
        if not n.isascii() or not n.isdecimal() or n.startswith("0") or len(n) > 2:
            return _line("ERR code=bad_arg")
        index = int(n)
        if not snapshot.caps_fresh or snapshot.pd_state in ("detached", "recovery"):
            return _line("ERR code=caps_unavailable")
        if not (1 <= index <= len(snapshot.raw_pdos)):
            return _line("ERR code=out_of_range")
        return _line(_decode_cap(index, snapshot.raw_pdos[index - 1]))
    if req == "STATUS":
        if not snapshot.contract_valid or snapshot.pd_state in ("detached", "recovery"):
            return _line(f"STATUS state={snapshot.pd_state} v=0 pps=0 pos=0 mv=0 ma=0")
        return _line(f"STATUS state={snapshot.pd_state} v=1 pps={int(snapshot.pps_active)} pos={snapshot.pdo_index} mv={snapshot.requested_mv} ma={snapshot.requested_ma}")
    if req == "MEASURE":
        s = snapshot.sensor
        if not s.valid:
            return _line("MEASURE status=error reason=sensor_unavailable")
        return _line(f"MEASURE status=ok voltage_mV={s.voltage_mv} current_mA={s.current_ma}")
    if req.startswith("SET_") or req.startswith("OUTPUT_"):
        return _line("ERR code=read_only")
    return _line("ERR code=unknown_command")


__all__ = ["MAX_LINE", "MAX_CAPS", "Snapshot", "Sensor", "UsbHealth", "handle_request"]
