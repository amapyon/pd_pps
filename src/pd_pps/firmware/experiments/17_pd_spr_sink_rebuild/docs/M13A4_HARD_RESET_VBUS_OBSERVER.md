# PD17 M13a-4 — Hard Reset VBUS transition observer (diagnostic only)

## Base and scope

Overlay on the **existing M13a-3** `17_pd_spr_sink_rebuild` folder. Do not apply this
package to an older firmware milestone. Only the numbered 17 experiment is changed;
`16b_source_caps_integrity` remains untouched.

This milestone is **NOT USB PD Hard Reset autorecovery**. The current PHY still
latches `hard_reset`, suppresses GoodCRC and PD transmissions, and waits for
physical detach/re-attach. The Policy Engine remains `PE_RECOVERY` after Hard Reset.
The new observer cannot clear either latch or queue a Request. Do not enable
voltage-only re-attach based on the observation status.

Reference: user-provided USB PD 3.2 r3.2 v1.2 (Aug 19 2026), section 4.4
(Hard Reset sequence) and section 9.2.4.9 (Sink Transition to Default).
The USB PD electrical definitions include vSafe0V(max) 0.8V and vSafe5V
4.75–5.5V; the raw ADC windows below are intentionally **stricter provisional
measurement thresholds**, not validated compliance thresholds.

## What changed

- `src/pd_hr_vbus.[ch]`: independent, pure-C diagnostic state machine.
- `src/pd_vbus_monitor.[ch]`: feeds valid conversions to this observer after
  Hard Reset; excludes samples started before reset; errors on ADC failure;
  retains result for detached-only printing.
- `src/main.c`: starts the observer ONLY after `PE_EV_HARD_RESET_RX`.
- `Makefile`: includes new `src/pd_hr_vbus.c`.
- No Policy Engine or PHY send/retry/recovery behavior changes.

## Observer states / error codes

| `PD17 HR VBUS state` | Interpretation |
|---|---|
| 0 | IDLE: no Hard Reset observed in this connection |
| 1 | WAIT_LOW: Hard Reset seen, waiting for 0V-near ADC samples |
| 2 | WAIT_5V: 0V-near samples seen, waiting for 5V-near samples |
| 3 | 5V_OBSERVED: both levels observed in the correct order; **TX remains disabled** |
| 4 | FAILED: timeout or ADC failure |
| 5 | DETACHED: physical CC detachment before observation complete |

`error=1`: low-voltage timeout; `error=2`: 5V return timeout;
`error=3`: ADC read/start failure. Normal physical unplug without Hard Reset
will report `state=0` and `auto_recovery=0`.

## Provisional thresholds and caveats

- ADC 10ms nominal interval, 3 consecutive samples, gap between samples <=25ms.
- Raw <=70 for provisional low (0.62V using nominal 3.3V/12-bit/11:1 divider).
- Raw 540..610 for provisional return to ~5V (4.78–5.40V nominal).
- Low timeout 1000ms from the PE Hard Reset event.
- 5V return timeout 2000ms from the confirmed low indication.
- A sensor timeout (>=3ms) fails the observer.
- Any CC detach during the observation aborts it; no automatic restart.
- ADC offsets near zero, calibration vs. DMM, actual Source Hard Reset waveform,
  DC-DC power supply behavior, and CC comparator reaction during a reset are
  not yet validated. One voltage sample does not establish electrical compliance.

The observer has no effect on the existing M13a-3 **early VBUS loss** detection
used for physical cable pulls while Ready. In an actual Hard Reset, the
`phy_hard_reset_seen()` latch suppresses the early-loss abort path.

## Tested in this artifact generation session

Host C code (GCC):

```
sh tests/run_m13a4_host_test.sh
```

Tests: 0V-before-5V sequencing, consecutive samples, out-of-range values,
missing transitions, ADC failure, detach, 32-bit ms rollover, missed-sample
streak reset, ADC integration and pre-Hard-Reset stale conversion exclusion.

The existing M12b, M12c, M12d, M13a, M13a-2, M13a-3 host scripts passed.
Clang UndefinedBehaviorSanitizer tests for the new code also passed.
**Windows/xPack Build, Flash, and actual Hard Reset have NOT been tested.**

## Safe first hardware step

With UGREEN 90551 USB-C1 **disconnected**, keep WCH-LinkE connected. Stop
any active Log (Ctrl+C), expand archive at the project root, then:

```
Set-Location 'C:\pj\pd_pps\src\pd_pps'
$Project = '17_pd_spr_sink_rebuild'
$ZIP = 'C:\pj\pd_pps\PD17_M13a4_hr_vbus_observer.zip'
Expand-Archive -LiteralPath $ZIP -DestinationPath . -Force
.\dev-workspace\pd-pps.ps1 -Action Build -Project $Project
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
.\dev-workspace\pd-pps.ps1 -Action Flash -Project $Project
if ($LASTEXITCODE -ne 0) { throw 'Flash failed' }
.\dev-workspace\pd-pps.ps1 -Action Log
```

Observe detached heartbeat only. **Do NOT intentionally provoke Hard Reset yet.**
A subsequent normal 7V/1A PPS connection and physical detach test is the
next experiment only after Build, Flash and heartbeat pass. The detached-only
summary adds `PD17 HR VBUS state=0 ... auto_recovery=0` on that normal test.

## Next milestones

M13a-5: establish an intentional but safe Hard Reset test method (and verify
retained 3.3V MCU/debug power, CC behavior, 0V/5V sensor windows). Only after
those observations should the team consider gating a PHY/Policy Engine restart
on the verified transitions and applying Sink-specific state/timer behavior.
