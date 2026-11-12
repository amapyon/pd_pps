# Experiment 23 — M8E USBFS IRQ-entry trace (independent)

Purpose: split host Bulk IN URB behavior from CH32X035 USBFS raw IRQ-entry activity.
This diagnostic only **observes** EP3 transfers; it does not implement automatic recovery.

## Safety / scope

- Existing experiment 21 and 22 sources stay untouched. This entire folder is new.
- Windows PC data connector and CoreBoard USB-PD J1 MUST NOT be plugged simultaneously.
- INA226 is powered from 3.3V, SCL=PA2, SDA=PA3; IN+=independent current-limited ~5V
  supply, IN-/VBS=external 100-ohm load side, common GND. No CoreBoard VBUS branch.
- Do not short IN- to ground while 5V is energized. Check resistor power rating and
  reference DMM in DC voltage mode; do not infer absolute accuracy from this test.
- Unlike experiment 22, this experiment instruments the **local copied** ch32fun fsusb.c;
  the project-wide `ch32fun/extralibs/fsusb.c` is never edited.
- The diagnostic ISR hook adds CPU work in IRQ context. Its timing can perturb the
  failure; PASS does not mean the original firmware is fixed.
- A USBPcap or Wireshark driver is NOT bundled or installed by this package.

## Raw USBFS IRQ trace

`m8e_irq_trace` in SRAM, magic 0x4D384952 (M8IR), ABI v1, 2120 bytes:
- Total IRQ entries, transfer events, EP3 IN callbacks observed at **IRQ entry**,
  TOG_OK/TOG_BAD, EP2 OUT, EP0 SETUP, bus reset, suspend, other transfers.
- Last 64 raw IRQ entries (32 byte each) with raw INT_FG + INT_ST,
  endpoint / token, EP3 controller and context status.
- The original experiment-22 `m8d_usb_trace` (ABI v1) is retained to
  compare raw IRQ entry against application completion callbacks.
- The hook is injected immediately after original `fsusb.c` reads the combined
  interrupt register. If the upstream source changes, the installer aborts
  instead of applying the hook to an unknown location.

A missing EP3 IN record does **not** by itself prove Windows never sent an IN
request. USBPcap records URBs, not low-level wire tokens.

## Install/build/flash

Extract the two-file kit under `C:\pj\pd_pps`. First run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Install-M8E-Only.ps1
```

Installer checks ZIP SHA256, experiment-22 baseline SHA256, Git HEAD, exact
vendor USBFS IRQ anchor and verifies the new directory does not exist.
It only adds `firmware/experiments/23_usb_cdc_irq_trace`; no Git commit/push,
no Flash and no existing files are changed.

```powershell
Set-Location C:\pj\pd_pps\src\pd_pps
$P = 'firmware/experiments/23_usb_cdc_irq_trace'
.\dev-workspace\pd-pps.ps1 -Action Build -Project $P
# ONLY after PASS and hardware review:
.\dev-workspace\pd-pps.ps1 -Action Flash -Project $P
```

Firmware INFO must report `firmware=m8e_usb_cdc_irq_trace protocol=5`.

## Host USBPcap / Wireshark

Windows USBPcap captures Windows USB Request Blocks (URBs), NOT wire-level
physical tokens. Wireshark's USB guide: https://wiki.wireshark.org/CaptureSetup/USB

1. Install the Wireshark release including USBPcap from a trusted publisher
   if not already installed. Installing a kernel driver may need administrator
   rights and reboot. Be aware USBPcap can affect other connected USB devices;
   work only on a test PC. Do not install unsolicited download mirrors.
2. Verify in Device Manager that the test CDC device is attached (VID `1209`,
   PID `D035`). Its COM number alone does not identify the root hub.
3. In Wireshark, select the **USBPcap root hub** containing this device.
   Use capture options to select the single attached device (or enable only
   newly connected device after starting capture). Do not indiscriminately
   capture keyboard, mouse or other sensitive USB traffic.
4. Start the capture **before** the serial test, run below in a second console,
   stop capture immediately after the timeout and save `.pcapng`.
5. View display filter: `usb.device_address == N && usb.endpoint_address == 0x83`
   where N is the actual USB address observed. If fields are absent, use
   `usb` and inspect device configuration / endpoint transactions.
   Inspect URB submission/completion, status and cancellation; an electrical
   IN token cannot be inferred from USBPcap alone.
6. Alternatively run `Start-M8E-USBPcap.ps1` in an elevated terminal **after**
   learning the correct root hub `\\.\USBPcap1` and device address from the
   Wireshark device list. Never guess either argument. Stop with `q` in USBPcapCMD.

## Host stress and device SRAM capture

```powershell
Set-Location C:\pj\pd_pps\src\pd_pps
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\firmware\experiments\23_usb_cdc_irq_trace\Test-M8E-Stall-Isolation.ps1 -Port COM9 -Mode MeasureOnly -Count 30 -IntervalMs 250
```

On timeout do not reset/reflash, preserve the host PCAP (stop capture) and
acquire SRAM with WCH-LinkE (debugger may halt/reset MCU):

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\firmware\experiments\23_usb_cdc_irq_trace\Capture-M8E-UsbTrace.ps1 -Port COM9 -State Failed
```

Decode the ZIP on Windows with Python:

```powershell
python .\firmware\experiments\23_usb_cdc_irq_trace\Decode-M8E-Trace.py 'C:\pj\pd_pps\diagnostics\M8E-USBFS-IRQ-Failed-<timestamp>.zip' --show 40
```

If host PCAP shows a Bulk IN URB submitted but not completed and IRQ trace
shows no EP3 IN, the source of loss lies **somewhere between** host transfer
submission and device completion; this alone does not distinguish host,
physical-layer and controller problems. An EP3 raw IN IRQ without application
callback suggests a local ISR handler/TOG_OK or callback routing question.

## Offline regression tests

```bash
cd firmware/experiments/23_usb_cdc_irq_trace
gcc -std=c11 -Wall -Wextra -Werror -I. -DM8E_IRQ_HOST_TEST host_test/test_irq_trace.c m8e_irq_trace.c -o /tmp/m8e_irq_test
/tmp/m8e_irq_test
gcc -std=c11 -Wall -Wextra -Werror -I. host_test/test_trace.c -o /tmp/m8e_trace_test
/tmp/m8e_trace_test
gcc -std=c11 -Wall -Wextra -Werror -I. host_test/test_measure.c -o /tmp/m8e_measure_test
/tmp/m8e_measure_test
```

Build and Flash are NOT executed by the installer, and they have not been
validated here on the CH32X035 Windows toolchain. No Git changes are staged.
