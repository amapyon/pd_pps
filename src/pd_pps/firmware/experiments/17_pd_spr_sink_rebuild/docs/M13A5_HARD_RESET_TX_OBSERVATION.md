# PD17 / M13a-5: Sink-originated Hard Reset – one-shot VBUS observation

## Scope

This changes only the independent `17_pd_spr_sink_rebuild` experimental
firmware. `16b_source_caps_integrity` must stay untouched.

A special build-time switch in `src/main.c` is **ENABLED**:

```c
#define PD17_M13A5_ENABLE_HARD_RESET_TX 1u
```

**WARNING: The firmware will deliberately transmit ONE USB PD Hard Reset
per MCU boot once the PPS contract, the Get_Source_Cap re-evaluation, and
another 500ms in READY have all completed.** It is *not* passive monitoring.
This is a controlled bench experiment, NOT general-purpose firmware.
Do not install it into a product or with an external load connected.

## Hardware / power prerequisites

- UGREEN 90551 USB-C1 only, other ports idle.
- WeActStudio CH32X035 CoreBoard; WCH-LinkE supplying/retaining board's 3.3V
  and debug connection even if USB-C VBUS drops. Verify detached heartbeats.
- PA0 monitors VBUS via 100k/10k divider; INA226 used only for passive
  initial READY snapshot; no extra electrical load.
- No inline USB power meter: it may change CC behavior.
- No synchronous print / software I2C during the Hard Reset exchange.
- Host test cannot prove USBPD BMC waveform correctness, VBUS thresholds,
  or timing with the particular charger. Do not use this as a PD compliance claim.

## New signals

CH32X035 RM §21.2.7 provides independent selection of the four ordered-set
K-codes in `R8_TX_SEL`. The USB PD 3.2 v1.2 §5.3.1.2.1 defines Hard Reset as
RST1 + RST1 + RST1 + RST2. Therefore TX_SEL is programmed to `0xA9`,
`BMC_TX_SZ=0` (no SOP header/data/CRC), and USBPD TX is started once.
Hardware-specific encoding is not verified by a CC waveform analyzer;
`tHardResetComplete` is <=5ms in the USB PD specification. We use a 6ms
software diagnostic timeout. Even when this expires, we **never retry**.

## State machine and safety constraints

1. Only schedule after GET_SOURCE_CAP has actually resulted in a transition
   out of PE_READY and back to PE_READY with a valid PPS contract.
2. Maintain 500ms uninterrupted READY; a pending ADC sample may postpone the
   send but must not reset that 500ms timer.
3. An irrevocable one-shot latch forbids further Hard Reset TX attempts
   without rebooting the MCU; physical detach does NOT re-arm it.
4. Reject starting TX if CC detached, PHY TX busy, prior message awaiting
   GoodCRC, an earlier Hard Reset, or VBUS-loss latch is set.
5. When transmission starts, flush stale RX queue and retry timers; gate
   GoodCRC / further SOP messages, immediately invalidate PE's PPS contract.
6. Independently start M13a-4 passive PA0 VBUS observer. ADC 10ms history
   continues while the firmware's PHY is fail-closed.
7. On TX_END, record completion without arming CRCReceiveTimer. If no TX_END
   within 6ms, record a timeout and keep the protocol fail-closed.
8. **No source-capability-based or voltage-only automatic recovery.**
   Physical CC unplugging allows the ordinary detached debug log to drain.

## Trace codes

- `code=19` `PDTR_TX_HARD_RESET`: configured and started the Hard Reset TX;
  `aux=00a9`.
- `code=20` `PDTR_TX_HARD_RESET_END`: hardware completion indicated TX_END.
- `code=21` `PDTR_TX_HARD_RESET_TIMEOUT`: TX_END not seen within 6ms.
- `code=15 detail=11`: PE processed the locally initiated Hard Reset, now
  PE_RECOVERY (6), contract invalid.
- `PD17 HR VBUS state=3`: observer saw low (raw <=70) then 5V (raw 540..610),
  three consecutive valid samples each. This is an ADC indication, not
  an absolute VBUS conformance measurement. `state=4` is an error.
- `low_rel_hr_ms`, `high_rel_hr_ms`: offsets measured from foreground start
  near Hard Reset TX initiation (not exact EOP time).

## Tests run while producing this patch

Host tests invoke production `pd_phy_ch32x035.c`, `pd_policy.c`, and the
production `pd_hr_bench.c` via mocks; they cover CC1/CC2, K-code TX_SEL,
no SOP payload, one-shot guard, previous TX busy, RX Hard Reset, VBUS abort,
TX_END, timeout, 32-bit wrap, PE invalidation, ADC-busy eligibility and
500ms stable READY. All existing M12b/c/d and M13a through M13a-4 host tests
also passed. GCC and Clang UB sanitizer passed.

**NOT YET TESTED:** Windows/xPack Build, Flash, hardware K-code BMC waveform,
UGREEN response to locally initiated Hard Reset, MCU retention on power drop.

## First user experiment: NO USB-C charger

Keep the charger unplugged, end previous `Log` with Ctrl+C, and apply the
ZIP from `C:\pj\pd_pps\`:

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
$ZIP = 'C:\pj\pd_pps\PD17_M13a5_hr_one_shot_observer.zip'
$Project = '17_pd_spr_sink_rebuild'
Expand-Archive -LiteralPath $ZIP -DestinationPath . -Force
.\dev-workspace\pd-pps.ps1 -Action Build -Project $Project
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
.\dev-workspace\pd-pps.ps1 -Action Flash -Project $Project
if ($LASTEXITCODE -ne 0) { throw 'Flash failed' }
.\dev-workspace\pd-pps.ps1 -Action Log
```

STOP at detached heartbeat verification. **Do not attach USB-C yet.**

## After detached heartbeat PASS: controlled one-shot experiment (separate step)

With WCH-LinkE and Log running, charger USB-C1 and all other UGREEN ports
unloaded, attach the direct USB-C cable, leave plugged for roughly 6s to
allow the one-shot Hard Reset and low/5V observer to complete, then physically
unplug, do not reconnect until the entire `TRACE`, `SENSOR`, `HR VBUS`, and
`VBUS END` records are collected. A device brownout, missing heartbeat, or
abnormal charger/cable response means stop and document observations.

No automatic recovery is enabled, so this test is not expected to return to
PE_READY while USB-C remains connected after local Hard Reset. TX_END +
low/high ADC observations are distinct acceptance criteria; if TX_END occurs
without expected voltage movement this is a result to investigate, **not** a
reason to repeatedly reset the charger.
