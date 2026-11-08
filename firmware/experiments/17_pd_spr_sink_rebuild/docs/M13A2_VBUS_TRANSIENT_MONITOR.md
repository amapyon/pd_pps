# PD17 / M13a-2: VBUS transient monitor (diagnostics only)

## Intent and scope

Builds on the **M13a-1 ADC + INA226 integration** already validated on device
(READY raw=791, ADC uncalibrated=7012 mV, ADC calibrated=7033 mV,
INA226 BUS=6995 mV; detached ADC raw=15 / 133 mV).

- Maintain the existing SPR PPS 7000 mV / 1000 mA sink policy and M11b
  one-shot `Get_Source_Cap` experiment.
- Observe PA0 / ADC_IN0 VBUS every nominal 10 ms **without waiting** for ADC EOC.
- Retain the last 128 completed measurements (~1.28 s) in an overwriting RAM
  ring while the cable is connected and for 500 ms after confirmed CC detach.
- Preserve one-off READY ADC/INA226 sampling and detached ADC reading.
- Print **only after CC detach**, in small increments, after the existing PD
  trace and the `PD17 SENSOR ...` records.
- **No automatic Hard Reset recovery**, no vSafe0V/vSafe5V decisions. This is
  an observability-only milestone.

## Circuit and measurement limitations

- VBUS -> 100 kΩ -> PA0 -> 10 kΩ -> GND. Never connect VBUS to MCU PA0 directly.
- ADC voltage conversion assumes 3.3 V reference and a 12-bit ADC; the
  raw-to-VBUS conversion uses the 11:1 divider ratio. `uncal_mv` is an
  estimate, NOT a certified voltage.
- The previously calibrated conversion is valid only for ADC raw 457..1012,
  approximately 4..9 V. No use of calibrated fit for near-zero readings.
- INA226 uses software I2C on PA2/PA3 and remains **one-shot** ~300 ms after
  READY, not 100 Hz. Its physical BUS voltage may differ from the PA0 tap.
- ADC polling uses one conversion at a time; no ADC read, delay or printf in
  the USBPD IRQ. A conversion start and an EOC check are separate foreground
  operations. If EOC is not seen within 3 ms (millisecond-granularity timer),
  the continuous monitor stops and reports an error; PD handling continues.
- The original single READY ADC + INA226 read remains synchronous, once per
  CC session. This existing I2C acquisition is not made nonblocking here.
- Sampling can jitter due to main-loop work and USBPD activity; use the
  actual `rel_detach_ms` timestamps to interpret it.
- `rel_detach_ms=0` means *confirmed* CC detach, ~150 ms **after** the CC loss
  detection debounce begins. It does not label the physical connector
  removal time. Samples before confirmed detach have negative time offsets.
- Only the most recent 128 samples are printed. `total`, `overwritten`,
  and the min/max of samples whose software state was READY are retained for
  the entire session.

## Log format

After `PD17 TRACE END` and `PD17 SENSOR READY/DETACHED`:

```
PD17 VBUS BEGIN period_ms=10 count=... total=... overwritten=... timeouts=0 stopped_on_error=0 ready_n=... ready_min_raw=... ready_max_raw=... detach_seen=1
PD17 VBUS rel_detach_ms=-... raw=... uncal_mv=... flags=3
PD17 VBUS rel_detach_ms=+... raw=... uncal_mv=... flags=0
PD17 VBUS END
```

`flags` bitmask: `1=CC attached`, `2=PE READY with valid contract`,
`4=Hard Reset latched`. A physical unplug is not yet reflected as a CC
`DETACH` event during the debounce window; READY and ATTACHED bits may still
be set while the measured VBUS is already falling. This is expected.
`ready_min_raw` can include these falling-voltage samples.

No printing occurs during a PD connection. The `VBUS` report prints at most
one line per main-loop iteration after confirmed detach.

## Host test results

```
sh tests/run_m13a2_vbus_host_test.sh
sh tests/run_m13a_sensors_host_test.sh
sh tests/run_m13a_host_test.sh
sh tests/run_m12b_host_test.sh
sh tests/run_m12c_host_test.sh
sh tests/run_m12d_host_test.sh
sh tests/run_host_test.sh
```

All passed in the authoring environment. Tests cover ADC register mock,
conversion start vs completion, fixed-size history, overwrite, min/max,
interval, ADC timeout, Message ID/Protocol Engine existing behavior, and
32-bit timestamp wrap. They do NOT substitute for a Windows/xPack RISC-V
cross-build or for oscilloscope / electrical timing measurements.

## Device test gate 1: disconnected Build + Flash + heartbeat

Use the known M13a-1 flashed project, with UGREEN 90551 **USB-C1 unplugged**,
no external load, WCH-LinkE connected. Overlay only this ZIP's files onto
`C:\pj\pd_pps\src\pd_pps\` and build/flash project `17_pd_spr_sink_rebuild`.
Confirm `PD17: alive, charger detached, state=0, lost=0` several times.
No source connection in this first gate.

## Device test gate 2: normal PPS and cable-pull transient

Once gate 1 passes, keep the debug Log running and attach the same charger
USB-C1 directly (no inline meter). Leave it ~6 seconds with no load, then
remove the USB-C cable. Keep WCH-LinkE connected. Wait until **PD17 VBUS END**;
there may be ~128 additional lines after `PD17 TRACE END`. Do not reconnect
until printing finishes.

Expected: normal 7 V / 1 A PPS contract, initial READY, Get_Source_Cap,
periodic refresh, `trace_dropped=0`, READY ADC near 7 V and INA near 7 V.
VBUS ring should show approximately 7 V shortly before cable removal and a
low uncalibrated value afterward, without ADC timeouts. Exact edge/timing
requires inspection of observed timestamps, not a preselected threshold.

Do not deliberately trigger Hard Reset, short-circuit, or overload. This
experiment does **not** validate automatic recovery nor vSafe0V/vSafe5V.
