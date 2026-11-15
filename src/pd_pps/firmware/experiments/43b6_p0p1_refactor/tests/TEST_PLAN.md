# TEST_PLAN — M43B-5 PPS refresh / USBFS IRQ correlation

## Goal and baseline

- Target WeAct CH32X035F8U6, `ch32fun`, WCH-LinkE, Windows 11 / PowerShell 5.1.
- M43B-4 v01 verified cross-build/FLASH on user's PC; the earlier CDC ON
  high-rate attempt recorded PING=199353/199353, `fs_crc=29`, PPS_READY=1,
  refresh_started=1 and refresh_done=0; SRAM froze at first Source HR.
- M43B-5 must preserve first HR recovery evidence and capture a later refresh
  failure or second HR with per-Request USBFS CRC IRQ counts.

## Safety and instrumentation

- CDC ON + PD connected on CC1 is the primary test; do not require CDC OFF.
- PC/PD data lines must not be shared/shorted, USB VBUS must not back-feed,
  common GND/CC wiring must be correct. No external load.
- Avoid connected high-power devices; keep MCU supplied while reading SRAM.
- CRC window IRQ increments DO NOT by themselves prove causality.
- The first Source HR may be logged without freeze; pending later freeze
  must be `TX_FAILED`, `SOURCE_HARD_RESET` with `source_hr_count>=2`, or
  other documented fault/detach. A second HR may occur before PPS refresh,
  in which case refresh-failure evidence is NOT available; classify REVIEW.

## Host / Build / Flash (not hardware PD PASS)

- Use package `Run-Firmware.ps1` via one PowerShell 5.1 block: verify ZIP
  published SHA, expand, manifest, install dry-run, `APPLY`, host tests,
  CH32 cross-build, BIN hash/size, WCH-LinkE target probe, `FLASH`, log check.
- Do not silently reflash, overwrite M43B-4 experiments, or perform Git.
- Any nonzero exit code or mismatch stops subsequent steps. Apply/flash
  may have partially completed; no rollback is claimed.

## Individual sessions

- `M43B5_01_ON_CC1`: WCH-LinkE `minichlink -T` in terminal A, CDC PING
  high-rate with periodic STAT/COUNT in terminal B; connect PD charger CC1
  while CDC traffic is ALREADY running (capture explicit start/attach times).
  Run >= 20 seconds after attach; detach PD, keep MCU power on, close
  terminal, run Read-M43B5-SRAM.ps1. Collect PD trace, CDC log, SRAM BIN,
  `events.csv`, `summary.json`.
- PASS (diagnostic completeness): decoded SRAM has `REQ_BEGIN` phase
  PPS_REFRESH, related `REQ_WINDOW` and terminal `REQ_END` or a valid stop
  showing why unavailable. This is NOT a requirement for PD transfer PASS.
- FAIL (firmware test): unexpected reset, invalid SRAM signature/record
  bounds, invalid CH32X035, failed build/flash, data corruption, or unsafe
  power/USB conditions.
- REVIEW: session did not reach refresh, buffer overwrote needed events,
  request has no matching end, or high-rate CDC started too late.

Compare `M43B5_01_ON_CC1` with `M43B4_02_ON_CC1`, recording PING totals,
RTT, CDC overflow, PPS READY/refresh, goodcrc, TX_FAILED, fs_crc total,
per-window count and timestamp. Never infer Source GoodCRC absence from a
missing RX_ACT. Raw logs must not be overwritten, transformed or normalized.
