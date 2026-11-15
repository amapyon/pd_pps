#!/usr/bin/env python3
r"""PD/PPS Explorer: Windows-native, standard-library-only reference checks.

This checks Python reference rules and collected text logs. It DOES NOT execute
C firmware code, validate CH32X035 register semantics, or prove USB PD safety.

Windows PowerShell:
  py -3 pdpps_hostcheck.py
  py -3 pdpps_hostcheck.py --log path\to\minichlink-log.txt

No external packages, administrator privileges, WSL, USB access, or writes.
"""

from __future__ import annotations

import argparse
import re
import sys
import unittest
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

C1 = [0x0801912C, 0x0002D12C, 0x0003C12C, 0x0004B12C, 0x000641F4, 0xC1A42164]
DUAL = [0x0801912C, 0x0002D12C, 0x0003C12C, 0x0004B12C, 0x00164145, 0xC8DC215A]
C2 = [0x0801912C, 0x0002D12C, 0x0003C0FA, 0xC8DC2137]
BROKEN = DUAL[:-1] + [0x2F9AD2C3]
FIXED_ONLY = C1[:5]


def frame_length_valid(count: int, rx_bytes: int) -> bool:
    """M4-C *assumed* BMC byte count: header(2)+PDOs(4*n)+CRC(4)."""
    return 1 <= count <= 7 and 2 + 4 * count + 4 <= rx_bytes <= 34


def decode_fixed(word: int) -> Optional[tuple[int, int]]:
    if word >> 30 != 0:
        return None
    return (((word >> 10) & 0x3FF) * 50, (word & 0x3FF) * 10)


def decode_pps(word: int) -> Optional[tuple[int, int, int, bool]]:
    if word >> 30 != 3 or ((word >> 28) & 3) != 0:
        return None
    return (
        ((word >> 8) & 0xFF) * 100,
        ((word >> 17) & 0xFF) * 100,
        (word & 0x7F) * 50,
        bool((word >> 27) & 1),
    )


def caps_plausible(words: list[int]) -> bool:
    """Python reference matching current M4-C guard, not C execution."""
    if not 1 <= len(words) <= 7:
        return False
    first = decode_fixed(words[0])
    if first is None or first[0] != 5000 or first[1] <= 0:
        return False
    for word in words:
        fixed = decode_fixed(word)
        if fixed is not None:
            v, i = fixed
            if not (5000 <= v <= 20000 and i > 0):
                return False
            continue
        pps = decode_pps(word)
        if pps is not None:
            lo, hi, ma, _ = pps
            if not (lo >= 3300 and lo <= hi <= 21000 and ma > 0):
                return False
        # M4-C intentionally leaves non-fixed, non-PPS PDO types to PD layer.
    return True


def request_fits_pps(apdo: tuple[int, int, int, bool], mv: int, ma: int) -> bool:
    lo, hi, max_ma, _ = apdo
    return lo <= mv <= hi and mv % 20 == 0 and 50 <= ma <= min(max_ma, 6350) and ma % 50 == 0


def pack_pps_rdo(mv: int, ma: int, object_position: int = 4) -> int:
    """Reference bit packing; not a check of the firmware's C bitfield ABI."""
    return ((object_position & 0xF) << 28) | (3 << 24) | ((mv // 20) << 9) | (ma // 50)


@dataclass
class CapRecord:
    advertised_count: int
    words: dict[int, int] = field(default_factory=dict)
    flags: list[str] = field(default_factory=list)
    bus_v: Optional[float] = None
    pps_restored: bool = False

    def summary(self) -> str:
        if len(self.words) != self.advertised_count or set(self.words) != set(range(self.advertised_count)):
            status = "INCOMPLETE (ログ欠落またはPDO不一致)"
        else:
            values = [self.words[i] for i in range(self.advertised_count)]
            status = "PLAUSIBLE" if caps_plausible(values) else "SUSPICIOUS PDO"
        pps_count = sum(decode_pps(word) is not None for word in self.words.values())
        description = f"count={self.advertised_count} parsed={len(self.words)} status={status} PPS_APDO={pps_count}"
        if self.bus_v is not None:
            description += f" BUS={self.bus_v:.3f}V"
        if self.pps_restored:
            description += " restore_PS_RDY=yes"
        if self.flags:
            description += " flags=" + ";".join(self.flags)
        return description


def parse_log(text: str) -> list[CapRecord]:
    records: list[CapRecord] = []
    current: Optional[CapRecord] = None
    count_re = re.compile(r"PD:\s+Source PDO count=(\d+)")
    pdo_re = re.compile(r"PD:\s+PDO\[(\d+)\]\s+raw=0x([0-9A-Fa-f]{8})")
    bus_re = re.compile(r"INA226:.*?\bBUS=(-?\d+(?:\.\d+)?)\s*V")
    for line in text.splitlines():
        match = count_re.search(line)
        if match:
            current = CapRecord(int(match.group(1)))
            records.append(current)
            continue
        if current is None:
            continue
        match = pdo_re.search(line)
        if match:
            current.words[int(match.group(1))] = int(match.group(2), 16)
        match = bus_re.search(line)
        if match:
            current.bus_v = float(match.group(1))
        if "restore PS_RDY" in line:
            current.pps_restored = True
        if "unexpected PD state" in line or "timeout" in line.lower():
            current.flags.append(line.strip())
    return records


class ReferenceTests(unittest.TestCase):
    def test_frame_sizes(self):
        self.assertTrue(frame_length_valid(6, 30))
        self.assertTrue(frame_length_valid(4, 22))
        self.assertTrue(frame_length_valid(7, 34))
        self.assertFalse(frame_length_valid(6, 26))
        self.assertFalse(frame_length_valid(8, 38))
        self.assertFalse(frame_length_valid(0, 6))

    def test_fixed_pdo(self):
        self.assertEqual(decode_fixed(0x0801912C), (5000, 3000))
        self.assertEqual(decode_fixed(0x000641F4), (20000, 5000))
        self.assertEqual(decode_fixed(0x2F9AD2C3), (34600, 7070))

    def test_apdo_decoding(self):
        self.assertEqual(decode_pps(0xC1A42164), (3300, 21000, 5000, False))
        self.assertEqual(decode_pps(0xC8DC215A), (3300, 11000, 4500, True))
        self.assertEqual(decode_pps(0xC8DC2137), (3300, 11000, 2750, True))

    def test_source_normal_single_and_multi_port(self):
        for words in (C1, DUAL, C2):
            with self.subTest(words=words):
                self.assertTrue(caps_plausible(words))

    def test_fixed_only_is_not_corrupt(self):
        self.assertTrue(caps_plausible(FIXED_ONLY))
        self.assertFalse(any(decode_pps(w) is not None for w in FIXED_ONLY))

    def test_anomalous_34v_pdo_rejected(self):
        self.assertFalse(caps_plausible(BROKEN))

    def test_invalid_first_pdo(self):
        self.assertFalse(caps_plausible([0] + C1[1:]))

    def test_no_pdos_or_too_many(self):
        self.assertFalse(caps_plausible([]))
        self.assertFalse(caps_plausible(C1 + C1[:2]))

    def test_invalid_pps_limits(self):
        bad = C1.copy()
        bad[5] = (0xC0000000 | (1 << 17) | (33 << 8) | 10)
        self.assertFalse(caps_plausible(bad))

    def test_request_on_source_c1(self):
        apdo = decode_pps(C1[5]); assert apdo is not None
        self.assertTrue(request_fits_pps(apdo, 7000, 1000))
        self.assertFalse(request_fits_pps(apdo, 7010, 1000))
        self.assertFalse(request_fits_pps(apdo, 7000, 1025))

    def test_request_on_dual_usb_c(self):
        apdo = decode_pps(DUAL[5]); assert apdo is not None
        self.assertTrue(request_fits_pps(apdo, 7000, 1000))
        self.assertFalse(request_fits_pps(apdo, 12000, 1000))
        self.assertTrue(apdo[3])  # Power Limited

    def test_rdo_vectors_from_existing_c_tests(self):
        self.assertEqual(pack_pps_rdo(7000, 1800), 0x4302BC24)
        self.assertEqual(pack_pps_rdo(7000, 1000), 0x4302BC14)
        self.assertEqual(pack_pps_rdo(3300, 50), 0x43014A01)
        self.assertEqual(pack_pps_rdo(21000, 3000), 0x4308343C)

    def test_log_normal_six_pdos(self):
        lines = ["PD: Source PDO count=6"] + [f"PD: PDO[{i}] raw=0x{w:08X}" for i, w in enumerate(C1)] + ["PPS: restore PS_RDY at requested 7000 mV / 1000 mA", "INA226: addr=0x40 BUS=6.988 V"]
        record, = parse_log("\n".join(lines))
        self.assertIn("PLAUSIBLE", record.summary())
        self.assertIn("PPS_APDO=1", record.summary())
        self.assertTrue(record.pps_restored)

    def test_log_corrupted_sixth_pdo(self):
        lines = ["PD: Source PDO count=6"] + [f"PD: PDO[{i}] raw=0x{w:08X}" for i, w in enumerate(BROKEN)]
        record, = parse_log("\n".join(lines))
        self.assertIn("SUSPICIOUS", record.summary())

    def test_log_five_fixed_pdos(self):
        lines = ["PD: Source PDO count=5"] + [f"PD: PDO[{i}] raw=0x{w:08X}" for i, w in enumerate(FIXED_ONLY)]
        record, = parse_log("\n".join(lines))
        self.assertIn("PLAUSIBLE", record.summary())
        self.assertIn("PPS_APDO=0", record.summary())

    def test_log_missing_pdo(self):
        lines = ["PD: Source PDO count=6"] + [f"PD: PDO[{i}] raw=0x{w:08X}" for i, w in enumerate(C1[:5])]
        record, = parse_log("\n".join(lines))
        self.assertIn("INCOMPLETE", record.summary())

    def test_ugreen_x756_t09_saved_fixture(self):
        path = (
            Path(__file__).resolve().parent
            / "testdata"
            / "ugreen_x756_t09_abnormal.log"
        )
        records = parse_log(path.read_text(encoding="utf-8-sig"))

        self.assertEqual(len(records), 2)
        first, second = records

        self.assertEqual(first.advertised_count, 6)
        self.assertEqual(first.words[5], 0x2F9AD2C3)
        self.assertIn("SUSPICIOUS PDO", first.summary())
        self.assertIn("PPS_APDO=0", first.summary())
        self.assertAlmostEqual(first.bus_v, 0.011, places=3)

        self.assertEqual(second.advertised_count, 5)
        self.assertIn("PLAUSIBLE", second.summary())
        self.assertIn("PPS_APDO=0", second.summary())
        self.assertAlmostEqual(second.bus_v, 5.091, places=3)

    def test_log_multiple_snapshots(self):
        block = lambda data: "\n".join([f"PD: Source PDO count={len(data)}"] + [f"PD: PDO[{i}] raw=0x{w:08X}" for i, w in enumerate(data)])
        records = parse_log("\n".join([block(C1), block(DUAL), block(FIXED_ONLY), block(C1)]))
        self.assertEqual([r.advertised_count for r in records], [6, 6, 5, 6])
        self.assertEqual([sum(decode_pps(w) is not None for w in r.words.values()) for r in records], [1, 1, 0, 1])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, help="Analyze a saved minichlink text log, without USB communication")
    args = parser.parse_args()
    if args.log:
        log = args.log.read_text(encoding="utf-8-sig", errors="replace")
        records = parse_log(log)
        print(f"Source Capabilities records: {len(records)}")
        for index, record in enumerate(records, 1):
            print(f"[{index}] {record.summary()}")
        return 0 if records else 2
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ReferenceTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    print("\nIMPORTANT: Python oracle tests only. Existing 14 C host tests remain unexecuted.")
    print("CH32X035 BMC_BYTE_CNT semantics and IRQ timing require separate validation.")
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
