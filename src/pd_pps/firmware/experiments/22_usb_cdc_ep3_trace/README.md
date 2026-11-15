# Experiment 22 — M8D USB CDC EP3 trace (independent diagnostic)

**Purpose:** Investigate the intermittent USB CDC bulk IN EP3 stall seen on the
WeActStudio CH32X035F8U6 CoreBoard during experiment 21 (M8 INA226 + USB CDC).
This is a diagnostic branch, **not** a new product milestone or a proven fix.

**Source baseline:** Experiment 21 at Git `902206a`. Experiment 21, experiments
16b/18/19/20, ch32fun and the roadmaps are **NOT modified**.

## Observed failure motivating this experiment

On 2026-11-10, two failing captures showed 1 more accepted EP3 send than
IN completion callbacks, with `USBFSCTX.endpoints[3].busy=1`. The latest
case timed out on the 8th `MEASURE` reply at a 250-ms request interval.
The MCU had built the reply (`voltage_mV=4945 current_mA=54`) and accepted
the 48-byte USB IN send, but Windows did not receive a complete reply within
3 seconds. Root cause **is NOT established**; electrical, host, controller,
interrupt, and library interactions remain open hypotheses.

## Modified behavior compared with experiment 21

- Same real INA226 driver and PA2/PA3 software I²C, R010=0.010 ohm.
- Same CDC ASCII commands: `INFO`, `STATUS`, `MEASURE`, `PING`, `HELP`.
- `INFO` now identifies `firmware=m8d_usb_cdc_ep3_trace protocol=4`.
- Build target is `m8d_usb_cdc_ep3_trace`.
- Adds `m8d_usb_trace` in SRAM: magic `M8TR` (`0x4D385452`), version `1`.
- Separate 32-entry event rings for **foreground** and **USB IRQ callback**.
- Every USB EP3 send attempt records pre-send and accepted/rejected snapshots.
- The EP3 IN completion callback records a snapshot **before** `fsusb.c`
  releases `busy`. A snapshot with `busy=1` in this callback is EXPECTED.
- EP2 OUT reception and USB class-specific setup callbacks are also recorded.
- The foreground loop records pending-EP3 snapshots even **after**
  `reply_sent==reply_length`, unlike experiment 21's old busy poll.
- It logs at wait counts 1, 256, 4096, 65536, 1048576, 16777216 and refreshes
  a most-recent hardware snapshot every 4096 main-loop iterations.
- There is **NO watchdog recovery, USB reset, automatic retry after a stuck
  host transfer, or CC/PD/PPS control**. Preserve failure for SRAM capture.

## Limitations of the trace

- `M8D_TraceIRQ` is called only from USB callbacks made by `fsusb.c`.
  It does **not** instrument the raw USBFS interrupt vector itself. Bus
  reset, suspend, all endpoint-0 transactions, missing interrupts, or
  USB errors that never reach a callback are **not exhaustively traced**.
- Foreground loop counts are **not milliseconds**, and two event rings do
  not provide a perfectly ordered shared timeline. ISR timing can occur
  between the foreground before/after snapshots.
- The debugger/probe may stop or reset the target. SRAM tells us what was
  visible when read, not necessarily what the bus was doing immediately
  before the host timeout.
- A diagnostic build may change the original timing (Heisenbug). A successful
  test with this build does NOT prove that the original M8 has been fixed.
- Ring buffers retain the most recent 32 events per producer.
- No USB protocol analyzer or host-side USB packet trace is supplied.

## Hardware boundary and wiring

Refer to `docs/HARDWARE_WIRING.md` in the repository.

- CH32X035 PA2=SCL, PA3=SDA; 3.3V sensor supply and external I2C pull-ups
  to 3.3V; ground common. VBS is wired to IN-.
- For a 5V/100-ohm **1 W** known-load test: independent current-limited supply `+`
  to INA226 `IN+`, INA226 `IN-` to resistor `+` and INA226 `VBS`, resistor
  other end to supply `-` / common GND. No additional branch to CoreBoard
  VBUS. Use DC VOLTS mode for DMM measurements (in parallel).
- **Never use the prototype PC USB CDC port and CoreBoard's USB-PD J1
  simultaneously**, because D+/D- are shared nets.
- No 48V EPR; INA226 bus max 36V. Validate the R010 shunt and wiring
  limits before high-current tests. This diagnostic is 5V/small-load only.
- If hardware is presently in an abnormal/fault condition, remove stimulus
  before rewiring; do not short `IN-` to GND while energized from 5V.

## Installation

Download both `M8D-EP3-Trace-Experiment22.zip` and `Install-M8D-Only.ps1`
to `C:\pj\pd_pps`, and run the installer from Windows PowerShell:

```powershell
Set-Location C:\pj\pd_pps
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Install-M8D-Only.ps1
```

Installation is a **new directory only**:
`src\pd_pps\firmware\experiments\22_usb_cdc_ep3_trace`.
It stops if this destination exists. It verifies SHA-256 checksums and the
Git baseline; does not stage, commit, push, build, flash, reset or modify
other experiments.

## Build and flash (only after hardware checks)

```powershell
Set-Location C:\pj\pd_pps\src\pd_pps
$Project = 'firmware/experiments/22_usb_cdc_ep3_trace'
.\dev-workspace\pd-pps.ps1 -Action Build -Project $Project
# Check the build log, target name, flashing probe and safe wiring first.
# Flash changes the MCU, NOT the Git repository.
.\dev-workspace\pd-pps.ps1 -Action Flash -Project $Project
```

No cross-compilation or flashing was performed during package creation.
Host GCC tests below passed; the Windows ch32fun build and physical test
must be confirmed independently.

## Reproduction test (new firmware; not the old M8 script)

Use the USB CDC COM port assigned by Windows (previously COM9; it may change).
Run only one serial client at a time. Begin with 30 requests:

```powershell
Set-Location C:\pj\pd_pps
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\src\pd_pps\firmware\experiments\22_usb_cdc_ep3_trace\Test-M8D-Stall-Isolation.ps1 `
  -Port COM9 -Mode MeasureOnly -Count 30 -IntervalMs 250
```

On failure, the script writes a partial CSV and a text summary to
`C:\pj\pd_pps\diagnostics\m8d_ep3trace`. Do **not** reset, detach or
re-flash the MCU before the following capture, provided it is safe to keep
powered and connected.

## Capture fault SRAM (WCH-LinkE, minichlink)

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\src\pd_pps\firmware\experiments\22_usb_cdc_ep3_trace\Capture-M8D-UsbTrace.ps1 `
  -Port COM9 -State Failed
```

The capture tool reads `20,480` bytes of SRAM and packages it with the local
ELF, MAP, and source in a ZIP under `C:\pj\pd_pps\diagnostics`. It does
**not** intentionally reset or reflash the MCU; attaching WCH-LinkE may
still halt/reset it. It validates the known diagnostic source SHA, the ELF
header/symbol, and basic freshness, but does NOT prove the exact flashed
binary equals the local ELF. The probe must already be connected/available.

Optional offline analysis (Python 3, no third-party modules):

```powershell
python .\src\pd_pps\firmware\experiments\22_usb_cdc_ep3_trace\Decode-M8D-Trace.py `
  "C:\pj\pd_pps\diagnostics\M8D-EP3-trace-Failed-YYYYMMDD-HHMMSS-fff.zip"
```

The decoder checks the ABI against SRAM and uses the captured MAP symbol
address rather than assuming a fixed SRAM layout. Attach the resulting ZIP
and test text/CSV for detailed interpretation.

## Host-only regression (no hardware required)

```bash
cd firmware/experiments/22_usb_cdc_ep3_trace
gcc -std=c11 -Wall -Wextra -Werror -I. host_test/test_measure.c -o /tmp/m8d_cdc_test
/tmp/m8d_cdc_test
gcc -std=c11 -Wall -Wextra -Werror -I. host_test/test_backend.c m8_measurement.c -o /tmp/m8d_backend_test
/tmp/m8d_backend_test
gcc -std=c11 -Wall -Wextra -Werror -I. host_test/test_ina226.c -o /tmp/m8d_ina_test
/tmp/m8d_ina_test
gcc -std=c11 -Wall -Wextra -Werror -I. host_test/test_trace.c -o /tmp/m8d_trace_test
/tmp/m8d_trace_test
```

**Source changes are limited to this new experiment.** The product
firmware must not adopt this diagnostic without independent verification.
