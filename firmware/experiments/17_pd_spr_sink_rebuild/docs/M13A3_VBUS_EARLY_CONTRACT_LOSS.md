# PD17 / M13a-3: early VBUS-loss detection and fail-closed stop

## Scope

This is a **safety-stop experiment**, not USB PD vSafe0V/vSafe5V automatic
Hard Reset recovery. It builds on the M13a-2 ADC monitor that was validated
on the physical device with:

- PPS 7V / 1A, ADC raw 793 (calibrated 7051 mV), INA226 BUS 6998 mV.
- A cable pull caused ADC raw to step from ~792 to 19,17,18 while
  `flags=3` continued for approximately 140ms before debounced CC DETACH.
- 10ms samples, 128-point history, 0 ADC timeouts, no missed trace events.

**Goal:** when an *existing negotiated contract* is in `PE_READY`, a sustained
VBUS near-zero indication must invalidate the contract and cancel outgoing
PD messages *before* the 150ms CC-detach debounce completes.

## Implemented guard

- Source of truth: CH32X035 ADC_IN0 on PA0, **100k / 10k** divider;
  never connect VBUS directly to MCU input.
- A raw ADC count **<=90** corresponds to approximately 798mV for the
  assumed 3.3V reference, 4095 full scale and 11:1 divider.
- Require **three consecutive completed samples** (typically spaced 10ms)
  for the same physical attached, non-Hard-Reset, READY session. A high sample,
  prior to READY, CC DETACH, or Hard Reset breaks the sequence.
- This threshold is an intentionally conservative experimental **power-loss
  stop threshold**, not a verified USB PD electrical vSafe0V detection.
  The existing six-point ADC calibration is not extrapolated below 4V.
- When the guard trips, in the **foreground**, before any new `phy_maintain`
  retry or PE command: atomically with USBPD IRQ masked disable any TX,
  clear retry state and stale RX queue, latch a PHY no-transmit gate,
  invalidate contract / PPS status / RDO and enter `PE_RECOVERY`.
- No GoodCRC or new Request is emitted in the stopped state. The latch can
  only clear through the existing **real CC detach + reattach** path.
- Hard Reset IRQ is given priority over a concurrent ADC loss stop; the PHY
  abort routine rechecks the reset latch inside the IRQ-masked region.
- Do not initiate a Hard Reset or automatically re-negotiate based on a
  returned ADC voltage. Those paths remain future work.
- The 10ms ADC history and INA226 one-shot measurements remain as in M13a-2.
  No printing / I2C from the ADC monitoring loop or USBPD IRQ.

## New diagnostic records

The post-detach trace may include:

```
PD17 TRACE ... code=18 detail=0 aux=0011 value=00000097
PD17 TRACE ... code=15 detail=10 aux=0004 value=00000006
PD17 VBUS BEGIN ... loss_detected=1 loss_raw=17 loss_rel_detach_ms=-123
```

- `code=18`: PHY `PDTR_VBUS_ABORT`; `aux` is raw ADC count;
  `value` is uncalibrated mV at detection.
- `code=15 detail=10`: `PE_EV_VBUS_LOST` (`PE_READY=4` -> `PE_RECOVERY=6`).
- The sample times and values above are **illustrative** and not predictions.
- `loss_rel_detach_ms` is the signed offset from **confirmed CC detach**, not
  physical plug removal. A normal test on this particular board might show
  a roughly -120ms value, but actual timing is assessed from the trace.

## Host verification

Run on Linux/macOS with host C compiler:

```
sh tests/run_m13a3_host_test.sh
sh tests/run_m13a2_vbus_host_test.sh
sh tests/run_m13a_sensors_host_test.sh
sh tests/run_m13a_host_test.sh
sh tests/run_m12b_host_test.sh
sh tests/run_m12c_host_test.sh
sh tests/run_m12d_host_test.sh
sh tests/run_host_test.sh
```

The new tests run the actual ADC monitoring logic, PE and USBPD PHY with mock
peripherals and cover: 3 sample boundary, one-sample glitch, threshold,
non-Ready/Hard Reset suppression, near-timeout/32-bit wrap, retry cancellation,
RX event purge, no-timer restart, CC rearm, and a Hard Reset IRQ race.
GCC and Clang UBSan pass for the two new test binaries.

These host tests do not prove physical ADC conversion timing, electronic
stability, or USB PD compliance. Windows/xPack Build and hardware tests are
pending.

## Real-device staged verification

Test setup: UGREEN 200W 90551 **USB-C1**, no other ports in use, directly
connected to the CH32X035 CoreBoard; no inline USB-C meter; no added load.
Keep WCH-LinkE powered during the unplug and all diagnostic output.

**Gate 1 (charger physically unplugged):** Stop existing `Log`, overlay the
ZIP at `C:\pj\pd_pps\src\pd_pps`, Build, Flash project
`17_pd_spr_sink_rebuild`, then `Log`. Verify repeated
`PD17: alive, charger detached, state=0, lost=0`.

**Gate 2 (only after Gate 1 PASS):** Keep `Log` running, plug USB-C1 for
approximately 6 seconds, then physically unplug. Do not reconnect until
`PD17 VBUS END`. Verify 7V/1A contract / Get_Source_Cap / PPS refresh, READY
ADC and INA226 around 7V, no ADC timeouts, VBUS falling to ~0.15V, and
`code=18`, `code=15 detail=10`, `loss_detected=1`. The PE fault count in the
final trace footer may be zero because DETACH initializes the PE. Use the
explicit VBUS abort trace and `loss_detected` to determine whether stop
actually occurred.

**Do not** deliberately trigger Hard Reset, short or overload for this test.
Do not infer Hard Reset autorecovery or full PD conformance from this result.

## Remaining issues for future milestones

- The guard is only active while `PE_READY` and a contract are valid, and is
  not yet a general undervoltage monitor during a pending PPS refresh AMS.
- It detects severe loss (~0.8V), not all unacceptable intermediate voltages.
- The ADC monitor currently stops on ADC timeout; a separate fail-safe sensor
  fault policy has not been validated.
- Hard Reset while CC remains attached requires electrical vSafe0V/vSafe5V
  detection with verified timing, SourceCap reinitialization and auto-recovery.
- This experimental firmware does not switch off the physical power output
  with an external load disconnect FET. Hardware protection remains needed.
