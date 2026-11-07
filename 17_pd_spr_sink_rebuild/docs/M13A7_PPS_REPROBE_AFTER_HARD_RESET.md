# M13a-7 — One Get_Source_Cap after Hard Reset fallback to fixed 5 V

## Scope and evidence

This change applies **only** to `17_pd_spr_sink_rebuild` **after M13a-6**.
`16b_source_caps_integrity` and earlier experiments remain unchanged.

**Observed on M13a-6 hardware (UGREEN 90551, USB-C1):**
- Before one-shot Sink Hard Reset, SourceCaps contains 6 PDOs including
  PPS APDO `0xc1a4213c`, and the Sink negotiates 7000 mV / 1000 mA.
- After Hard Reset, ADC confirmed near-zero at approximately 64 ms and 5 V
  at approximately 914 ms; the first post-reset SourceCaps contains only
  five fixed PDOs, and the Sink validly negotiates fixed 5 V / 1 A.
- The observed five-PDO capability set does **not** demonstrate that PPS
  is permanently unavailable. M13a-7 tests whether it reappears after
  the explicit 5 V contract has been stable for roughly one second.

## Behavior

1. **No new polling before Hard Reset recovery.** M11b's existing
   once-only first-Ready Get_Source_Cap remains unchanged.
2. After M13a-6 marks the Hard Reset recovery handoff completed, if the
   newly negotiated explicit contract is fixed 5 V / 1 A, wait for
   **1,000 ms of uninterrupted PE_READY** before asking for caps.
   This timer is reset when a different AMS interrupts the Ready state.
3. Schedule exactly **one** `PE_CMD_GET_SOURCE_CAP` through the existing
   `pe_request_source_caps()` / `phy_send()` path. Do not print during PD.
   If the command cannot start within 200 ms, cancel only this **unsent**
   command. If the Source sends no SourceCaps within 250 ms after sending,
   leave the existing explicit 5 V contract intact and do not query again.
   These are experimental bounds, not a claim of protocol conformance.
4. A **new** SourceCaps message is evaluated by the existing Policy Engine.
   A suitable PPS APDO results in RDO `0x6302bc14` (7 V / 1 A); Accept and
   PS_RDY are still required before PPS is considered active.
5. If it still contains only fixed PDOs, the existing Policy Engine
   responds to this **new SourceCaps** with a fixed-5V Request and
   returns to 5V Ready on Accept/PS_RDY. This is **one ordinary
   renegotiation**, not repeated probing. No further Get_Source_Cap is sent.
   SourceCaps reception normally triggers capability evaluation and a new
   power request in SPR; simply ignoring it is not a compliant shortcut.
6. If the Source sends unsolicited PPS-capable SourceCaps before the
   scheduled probe and 7 V is negotiated, skip the probe entirely.
7. On loss of connection, further Hard Reset, VBUS-loss latch, ADC monitor
   failure, or PE_RECOVERY, the probe stops. The M13a-6 one-shot Hard Reset
   and fail-closed recovery rules are unchanged.

## Distinguishing the outcomes in detached logs

The new deferred-only line is:

```
PD17 PPS REPROBE state=N sent=0/1 caps_seen=0/1 pps_offered=0/1 pps_ready=0/1 timeout=0/1
```

`state` values:
- `0`: not attempted; `1`: waiting for 1-second fixed5 Ready;
- `2`: queued but not yet started; `3`: sent, waiting for caps;
- `4`: response had no suitable PPS, existing fixed 5V path retained;
- `5`: suitable PPS offered, negotiation still in progress;
- `6`: PPS APDO was offered **and** 7 V/1 A Ready was reached;
- `7`: request/response timeout (fixed5 retained if PE did not fault);
- `8`: aborted due to fault or disconnect;
- `9`: PPS had already returned without our probe.

`PD17 TRACE code=26` reports these state transitions in `detail`. The
pre-existing `PD17 HR VBUS ... auto_recovery=1` still means a genuine PPS
explicit contract was re-established after Hard Reset, **not** merely a 5 V
Ready. To confirm actual voltage after probe, inspect the 10ms ADC history
near detach. The earlier `PD17 SENSOR READY` capture is generally sampled
before Hard Reset and does not prove post-recovery voltage.

## Host validation

```
cd 17_pd_spr_sink_rebuild
sh tests/run_m13a7_host_test.sh
```

M13a-7 tests cover: PPS returns and 7 V Ready; no PPS and fixed5 only;
no SourceCaps response; inability to queue a command; already-PPS skip;
Ready interruption and unsolicited PPS; 32-bit ms wrap, second Hard Reset,
detach; no probe before Hard Reset handoff.

Previous M12b/c/d and M13a through M13a-6 suites are also run in the
supplied development environment. Host tests do **not** validate actual
USB PD BMC signaling, WCH interrupts, or Source-specific behavior.

## Step 1 — only Build / Flash / detached heartbeat

1. Disconnect UGREEN's USB-C cable and stop `Log` (Ctrl+C). Keep WCH-LinkE.
2. Extract `PD17_M13a7_pps_reprobe.zip` into the repository root,
   **after** M13a-6. No other experiment folder is modified.
3. Build, Flash, and observe `PD17: alive, charger detached, state=0, lost=0`.
4. **Do not reconnect the charger before reviewing Build/Flash/heartbeat.**

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
$ZIP = 'C:\pj\pd_pps\PD17_M13a7_pps_reprobe.zip'
$Project = '17_pd_spr_sink_rebuild'
Expand-Archive -LiteralPath $ZIP -DestinationPath . -Force
.\dev-workspace\pd-pps.ps1 -Action Build -Project $Project
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
.\dev-workspace\pd-pps.ps1 -Action Flash -Project $Project
if ($LASTEXITCODE -ne 0) { throw 'Flash failed' }
.\dev-workspace\pd-pps.ps1 -Action Log
```

## Step 2 — only after passing Step 1 (separate experiment)

UGREEN 90551, USB-C1, other ports empty, no external load. Leave WCH-LinkE
Log running, attach for about **6 seconds**, then detach. Send the logs from
`PD17 TRACE BEGIN` through `PD17 VBUS END`, including `PD17 PPS REPROBE`.
This trial will again transmit **one deliberate Hard Reset automatically**.
Do not attach a sensitive 7V load, do not use an inline power meter, and
stop if any hardware anomaly occurs.
