# M43B-8 B: PPS control over PC USB CDC (experimental)

This experiment is constructed as a **new folder** from local M43B-8 A.
It preserves PHY, USBFS IRQ handlers, ADC safety, existing read-only commands,
and 7 V / 1 A boot behavior. It is NOT a production controller.

## Added serial commands (ASCII + newline)

- `PPS GET`: goal vs PS_RDY-confirmed contract voltage/current.
- `PPS STATUS`: `pending` (goal not yet confirmed), `applied` (PS_RDY-confirmed goal), PE state.
- `PPS SET 7020 1000`: requests new PPS target while PPS READY. `queued=1`
  acknowledges only foreground scheduling, NOT PD Accept/PS_RDY or VBUS.
- `PPS SET 7000 1000`: if already applied, returns `unchanged=1` without sending another Request.
- `ERR PPS range/not_ready/busy/unsupported/syntax`: no state change.

Restricted experimental range: 5,000-8,000 mV (20mV steps), 500-1,000 mA
(50mA steps), and within current Source APDO. **These are software gates,
not verified board electrical ratings.** Initial tests must be load-free and
use only 7,000/7,020mV, 1,000mA after checking the entire VBUS path rating.

`PPS STATUS applied=1` means Accept and PS_RDY were handled for the target.
This is *not* an independently calibrated DMM measurement.

## Build (Windows PowerShell, from source root)

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\pj\pd_pps\src\pd_pps\dev-workspace\pd-pps.ps1" -Action Build -Project firmware/experiments/43b8b_pps_control
```

Do not flash until native tests and build pass and board/cable safety is checked.
USB CDC is still COM16 only if Windows enumerates this device as COM16.

## Existing baseline behavior

On power-on or physical CC detach, the target defaults to 7,000mV/1,000mA.
No PC control when detached, fixed-5V fallback is active, in RECOVERY,
or during another AMS/refresh. Setting another target does not trigger
an immediate PD operation inside USB IRQ. The foreground session owns TX.
The original read-only PowerShell diagnostic script intentionally blocks
all write commands and must NOT be used to send PPS SET.
