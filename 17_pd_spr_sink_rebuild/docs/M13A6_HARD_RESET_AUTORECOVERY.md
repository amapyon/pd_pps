# M13a-6 — Hard Reset one-shot autonomous protocol and PPS reconnection

## Scope and experimental safety

Starting point: **the real M13a-5 ZIP**, not a clean-room rewrite. Only the independent
`17_pd_spr_sink_rebuild` folder is modified. `16b_source_caps_integrity` stays intact.

This is **bench-only** firmware: the previously enabled M13a-5 stimulus will still
transmit ONE Sink-originated Hard Reset after the Get_Source_Cap / Ready sequence,
and the new M13a-6 logic will automatically restart the Sink protocol and request
PPS 7000mV / 1000mA **if** the observed voltage sequence and source responses
pass the gates below. This is not fully compliant USB PD recovery code.

### Design / state transitions

1. Existing initial PPS contract and same-SourceCaps re-evaluation complete;
   the M13a-5 stimulus sends one Hard Reset (`code=19`, then `code=20`).
2. Invalidate the explicit contract; block SOP and GoodCRC. Initiate PA0 ADC
   Hard Reset observation. Do not continue periodic PPS updates.
3. Confirm **three timely samples** at `raw <= 70` (near vSafe0V); this is
   conservative experimental raw-ADC evidence, not certified vSafe0V.
4. Immediately after that low confirmation, enable PHY receive in an **early
   Source_Capabilities-only staging mode**. If SourceCaps arrives immediately
   upon 5V restoration, respond GoodCRC and stage its payload/MessageID.
   **Do not send a Request or notify Policy Engine in this stage.**
5. Confirm **three timely samples** with `raw 540..610` (approx 4.78..5.40V).
   Then clear the Hard Reset gate, reset SOP TX MessageID to zero, preserve
   staged RX MessageID to suppress retransmission duplicates, enqueue the
   staged capabilities and reenter `PE_WAIT_CAPS`.
6. Process SourceCaps (staged or newly received), build the ordinary PPS RDO,
   wait for GoodCRC, Accept and PS_RDY, and finally enter `PE_READY`.
7. `recovery_handoff=1` means voltage validation and PHY/PE restart happened.
   `auto_recovery=1` means a **new PPS explicit contract reached Ready**.
   These are deliberately separate from merely observing 0V->5V.
8. Terminal failures do not initiate additional Hard Resets. The **first**
   Hard Reset (locally sent or received) consumes the per-MCU-boot recovery
   permit. Any second Hard Reset, ADC failure, CC detach during a transition,
   TX Hard Reset timeout, or post-handoff Policy Engine timeout uses fail-closed.
   Physical CC detach still permits the original connection startup in future.

### Limitations

- Hard Reset observation thresholds are based on previous board ADC data,
  not a calibrated low-voltage compliance measurement; ADC sample period is
  10ms. The INA226 is **not** polled synchronously during recovery.
- While early RX is armed, only valid Source_Capabilities SOP messages are
  acknowledged and staged; other SOP recovery traffic is not a full spec
  protocol implementation. No power request occurs before 5V validation.
- The code does not implement a complete `HardResetCounter` or all source-
  initiated AMS/error paths; only a single controlled recovery is supported.
- Hardware build, interrupt interactions and actual early-source timing
  remain to be tested on Windows/CH32X035. Never interpret host tests as USB
  PD certification.
- No synchronous debugprintf until physical CC detach; trace buffer limited to
  127 records, so longer tests may incur `trace_dropped > 0`.

## Host tests

Run from `17_pd_spr_sink_rebuild` on a host with POSIX shell, GCC/Clang:

```sh
sh tests/run_m13a6_host_test.sh
```

Coverage: low/high sequence and deadlines, sample-gap rejection, 32-bit wrap,
recovery-per-boot latch, fail-closed guards, early SourceCaps GoodCRC+staging,
deduplication/MessageID reset, second Hard Reset purge, PHY fail-stop, host
syntax of actual main.c. Earlier M12b/c/d and M13a test suites were also run.

## First CH32X035 experiment — NO USB-C CHARGER

1. Remove USB-C charger from CoreBoard. End the old Log with Ctrl+C.
2. Expand this ZIP into the repository project root; files are relative to the
   existing `17_pd_spr_sink_rebuild` directory.
3. Build and Flash, observe detached heartbeat. **Do not connect the charger
   until this separate build/flash/heartbeat gate passes.**

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
$ZIP = 'C:\pj\pd_pps\PD17_M13a6_auto_recovery.zip'
$Project = '17_pd_spr_sink_rebuild'
Expand-Archive -LiteralPath $ZIP -DestinationPath . -Force
.\dev-workspace\pd-pps.ps1 -Action Build -Project $Project
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
.\dev-workspace\pd-pps.ps1 -Action Flash -Project $Project
if ($LASTEXITCODE -ne 0) { throw 'Flash failed' }
.\dev-workspace\pd-pps.ps1 -Action Log
```

## Later CH32X035 experiment — SEPARATELY AUTHORIZE AFTER HEARTBEAT

Use UGREEN 90551 **USB-C1**, other ports unused, no extra load, WCH-LinkE
powered. A single 4–5s direct cable connection should be long enough for the
initial 7V, one Hard Reset, and 0V->5V->7V recovery on the previously
observed charger; the user must physically detach and collect deferred logs.
Do not repeat if there is abnormal thermal behavior or brownout.

Relevant log codes:

| Code | Meaning |
| --- | --- |
| `19` / `20` | Hard Reset TX begin / TX_END |
| `22` | SourceCaps RX pre-armed after confirmed low VBUS |
| `25` | SourceCaps arrived during provisional RX and was staged |
| `23` | PHY reset gate released; `detail=1` indicates staged Caps |
| `15 detail=12` | Policy Engine got verified-VBUS restart event |
| `15 detail=4` | Policy Engine reached Ready after new contract |
| `24` | Fail-closed PHY stop after a failed recovery |

Detached report: `PD17 HR VBUS ... low_seen=1 ... high_seen=1 ...
recovery_handoff=1 auto_recovery=1` is the expected success shape.
Check SourceCaps, Request (RDO `0x6302bc14`), Accept/PS_RDY and ADC VBUS
approximately 7V in the same report. `auto_recovery=0` is a non-success
result, even if `state=3` and `recovery_handoff=1`.
