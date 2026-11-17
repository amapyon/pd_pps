# M43B-8 A experiment test plan (not a test result)

## Scope and baseline

- Baseline: `checkpoint/m43b7-p2` at `4e8939fcca5004e28042c69e591cb76ddf248725`.
- New experiment: `43b8_power_plan_refactor`.
- Hardware: WeAct CH32X035F8U6 CoreBoard, WCH-LinkE, separate PD sink and PC USB.
- Intended change: pure PDO/APDO selection and RDO construction extraction.
- Non-changes: PHY, USBFS IRQ, SRAM binary format, CDC wire output, timeouts.

## Host gates

| ID | Operation | Expected | Evidence |
|----|-----------|----------|----------|
| A01 | Compile/run `test_pd_power_runtime.py` | PPS, fixed fallback, invalid capability and target cases PASS | Host unittest log |
| A02 | Run `test_stage_a_parity.py` | P2 and new PE fields match over 32 seeds x 10,000 random steps and directed transitions | Host unittest log |
| A03 | Run pre-existing tests | No unexpected FAIL; source guards updated only for intended policy/target rename | Host unittest log |
| A04 | Build `.elf` / `.bin` with project-pinned ch32fun toolchain | Successful, no new diagnostics and resource bounds met | Build log, ELF and BIN SHA-256 |

## Hardware gates (NOT RUN by this package)

| ID | Safe procedure | PASS / REVIEW / STOP |
|----|----------------|----------------------|
| A05 | No external load, PC USB physically disconnected; connect PD source under existing M43B-7 R01 conditions | PASS if initial RDO and PE event series match baseline; REVIEW on missing or truncated log; STOP if abnormal VBUS/thermal behavior |
| A06 | PC USB physically connected but CDC idle; reproduce known R02 comparison | Compare the known GoodCRC success/failure path; do not reinterpret existing failure as success |
| A07 | PD detached, run existing `PING`, `STAT`, `COUNT`, `VBLOG`, `PDTRACE` host checks | Same output bytes/behavior as existing R03 |

Record actual MCU/board, source/port, cable, CC orientation, load, PC USB
physical connection, test start/end time, firmware BIN SHA-256, raw logs,
result and limitations. Keep existing source and raw logs immutable.

## Stop conditions

Stop on unexpected heating, unknown power path, recurrent ADC failure,
VBUS abnormality, protocol behavior outside known paths, wrong firmware
hash, missing pinned submodule or unavailable verification equipment.
No intentional short, uncontrolled overcurrent, reverse feed or unsafe fault
injection. Source-originated Hard Reset testing is deferred.

## Unverified

Windows PowerShell 5.1 installer, CH32 cross-build, independent BIN matching,
real-hardware parity, long-duration PPS stability, electrical safety,
existing GoodCRC and VBUS root causes. These are NOT made PASS by host tests.
