# Experiment 24 — M8F USB CDC automatic recovery (CH32X035F8U6)

Independent recovery experiment based on experiment 23 (`23_usb_cdc_irq_trace`).
This is **not a fix for a defective USB hub, cable or power supply**.
It provides **bounded endpoint timeout, controlled USB re-enumeration and a
Windows auto-reconnecting client**. No changes to PD negotiation or USB-PD
output control. No source changes to experiments 21–23, ch32fun, or user work.

## What it actually does

**MCU:**

- Uses the pinned, locally instrumented `m8e_fsusb.c` copied from experiment 23.
- On a USB bus RESET interrupt, discards any partial ASCII request and pending
  USB response. No stale response is replayed into a new host session.
- Tracks each EP3 IN submission from just **before** the submission, avoiding
  a race with a fast completion interrupt. Clears the pending flag in EP3 IN IRQ.
- When EP3 IN fails to complete for **2500 ms**, stops the local USB peripheral,
  removes the CH32X035 D+ pullup, waits **150 ms**, then executes `USBFSSetup()`
  in the foreground to force a new USB enumeration. **Never** manually clears
  only the EP3 `busy` flag or alters the USB data toggle bits.
- Preserves counter values `bus_resets`, `tx_timeouts`, `reconnects` in SRAM and
  exposes them via the `HEALTH` command.

**Windows host (`Monitor-M8F-Cdc.ps1`):**

- Uses `Get-PnpDevice` to find `VID_1209&PID_D035` USB CDC, not only a fixed COM
  number. Option `-Port` disambiguates multiple matching devices.
- Re-opens `SerialPort`, validates `INFO ... protocol=6`, queries `HEALTH`, and
  continues fresh `MEASURE`/`PING` requests after reconnect.
- Logs every successful reply and all disconnection events to files **outside Git**.
- **Keeps waiting by default** if the device is absent (`-MaxReconnectSeconds 0`).
  Optional `-MaxReconnectSeconds 90` gives a bounded test that fails clearly
  if the device does not return within 90 seconds. Cancel with Ctrl+C.
- Never invents measurements during an outage. Retries are for **idempotent**
  measurement commands only. This design must NOT be blindly reused for future
  non-idempotent PD/PPS output-control commands.

**Limits:** A loose contact, hub power cut, permanently disconnected cable, PC
sleep or dead MCU cannot be corrected in software. Automatic reconnection
requires a functioning physical path and a Windows process that remains alive.
Device-side auto re-enumeration is a prototype recovery mechanism, not a
USB-IF compliance result; it may not cure underlying host-controller faults.
The current PC USB connector has no wired VBUS-sense input; hardware VBUS
monitoring and electrical design work are required for production.

## Hardware safety

- Never connect the CoreBoard J1 PD source cable simultaneously with the
  additional PC USB-C data cable (shared D+/D− nets).
- Power the board from its previously validated independent path. Do not wire
  PC USB VBUS directly into a 3.3 V rail.
- INA226 PA2=SCL, PA3=SDA, VCC=3.3 V, GND=common. IN+=independent test
  supply +, IN−=VBS and load +, load −=supply GND. Do not connect IN+ to
  CoreBoard VBUS for this test.
- For the initial communication tests, use a validated **0 V** INA226 input.
  This experiment is not required to measure a live PD/PPS source.

## Build and flash (Windows PowerShell 5.1)

From `C:\pj\pd_pps`, unpack the kit then run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Install-M8F-Only.ps1
```

The installer verifies every file hash and the original experiment 23 main
source; copies its local `m8e_fsusb.c` into **a new directory only**.
It refuses to overwrite an existing experiment 24 directory.

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
$project = 'firmware/experiments/24_usb_cdc_auto_recover'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\dev-workspace\pd-pps.ps1' -Action Build -Project $project
# AFTER reviewing Build result and checking safe wiring:
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\dev-workspace\pd-pps.ps1' -Action Flash -Project $project
```

A successful USB CDC enumeration should reply:

```
INFO board=CH32X035F8U6 firmware=m8f_usb_cdc_auto_recover protocol=6
HEALTH bus_resets=... tx_timeouts=... reconnects=...
```

## Run automatic-recovery monitor

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File '.\firmware\experiments\24_usb_cdc_auto_recover\Monitor-M8F-Cdc.ps1' `
  -Mode MeasureOnly -Count 100 -IntervalMs 250 -MaxReconnectSeconds 90
```

If COM9 is known and there are multiple matching devices, specify `-Port COM9`.
Recordings go to `C:\pj\pd_pps\diagnostics\m8f_recovery` by default.

### Acceptance test

1. Build PASS, flash PASS and `protocol=6` identity.
2. A normal 100-reply run completes without timeouts.
3. In a separate controlled test, with wiring and power known safe, disconnect
   and reconnect **only the PC USB data cable** while the host monitor runs.
4. Confirm host announces DISCONNECTED then CONNECTED and resumes the next
   sample; missing time is present as an explicit event, not fake data.
5. Query `HEALTH` before/after to check bus reset or software reenumeration
   counters. A bus reset may occur without `tx_timeouts` incrementing.

Do not claim this solves the earlier PHY / hub fault or guarantees zero data loss.
Real-hardware acceptance was confirmed on 2026-11-12; preserve all unrelated user changes.

## Offline regression tests (GCC, no hardware)

```bash
cd firmware/experiments/24_usb_cdc_auto_recover
gcc -std=c11 -Wall -Wextra -Werror -I. host_test/test_measure.c -o /tmp/m8f_measure && /tmp/m8f_measure
gcc -std=c11 -Wall -Wextra -Werror -I. host_test/test_backend.c m8_measurement.c -o /tmp/m8f_backend && /tmp/m8f_backend
gcc -std=c11 -Wall -Wextra -Werror -I. host_test/test_ina226.c -o /tmp/m8f_ina226 && /tmp/m8f_ina226
gcc -std=c11 -Wall -Wextra -Werror -I. host_test/test_trace.c -o /tmp/m8f_trace && /tmp/m8f_trace
gcc -std=c11 -Wall -Wextra -Werror -DM8E_IRQ_HOST_TEST -I. host_test/test_irq_trace.c m8e_irq_trace.c -o /tmp/m8f_irq && /tmp/m8f_irq
gcc -std=c11 -Wall -Wextra -Werror -I. host_test/test_recovery.c -o /tmp/m8f_recovery && /tmp/m8f_recovery
```

## 2026-11-12 hardware acceptance

- Normal 100 MEASURE replies: **PASS 100/100; disconnects=0**.
- Interrupted run: **PASS 100/100; disconnects=3** at samples **6, 24, 64**. Host reconnected COM9 and continued without fabricated measurements.
- USBPcap confirmed three USB errors followed by re-enumeration and HEALTH/MEASURE replies.
- HEALTH (bus_resets / tx_timeouts / reconnects): **4/0/0 → 6/0/0 → 10/0/0 → 12/0/0**. MCU 2,500ms forced-reenumeration path was **not triggered**.
- Zero-input 0mV/0mA replies do not certify INA226 accuracy, or simultaneous PD+PC USB operation.

See [verification evidence](../../../docs/M8_USB_CDC_RECOVERY_VERIFICATION.md). Keep raw USBPcap/SRAM captures outside the public repository.
