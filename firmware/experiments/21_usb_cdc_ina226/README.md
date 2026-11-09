# Experiment 21 — M8 USB CDC + INA226 real measurements

Hardware: WeActStudio CH32X035 CoreBoard + CJMCU-226/INA226 with R010 (10 mΩ) shunt.
Source hardware reference: **experiment 11 `11_pps_ina226_monitor`**. M8 is a new independent
implementation; experiment 11 source is not modified, and its exact firmware files were not
included in this package. M7 experiment 20 provides the proven USB CDC command protocol.

> **Hardware wiring source of truth:** [`docs/HARDWARE_WIRING.md`](../../../docs/HARDWARE_WIRING.md), especially sections 4–7. **Hardware test log:** [`docs/M8_USB_CDC_INA226_VERIFICATION.md`](../../../docs/M8_USB_CDC_INA226_VERIFICATION.md). CoreBoard USB J1 and the added PC data connector must not be connected at the same time in this prototype.

## Wiring inherited from experiment 11

| CH32X035 | INA226 (CJMCU-226) |
| --- | --- |
| PA2 (software I²C SCL) | SCL |
| PA3 (software I²C SDA) | SDA |
| 3.3 V | VCC (sensor supply) |
| GND | GND |
| — | A0/A1 GND → 7-bit address `0x40` |

- **Open drain emulation**: drive LOW with push-pull output / release HIGH with GPIO input.
  The sensor board must provide pull-ups to **3.3 V**, never to 5 V.
- INA226 **IN+** on the positive supply/shunt input, **IN−** on the load side;
  **VBUS/VBS** on the load-side positive node, in accordance with the actual module wiring.
- The R010 shunt is **0.01 Ω**: at 5 A it dissipates **0.25 W**; verify the fitted
  resistor's power rating, heat sinking, connectors and traces before high current.
- INA226 bus common-mode range is **0–36 V**, so this is a USB PD SPR monitor,
  **not** a 48 V EPR monitor.
- PA2/PA3 can share the I²C bus with the OLED used in experiment 11, but this
  firmware neither initializes nor drives the OLED.
- The current experimental CoreBoard's existing USB-C J1 and PC CDC-C
  **share D+/D− nets**. NEVER connect J1 to a PD source simultaneously with
  the PC data connector for this experiment. Start with INA226 independently
  powered and a low-voltage bench source + resistive load on IN+/IN−, *not J1*.
- Do not connect PC USB 5 V to the 3.3 V rail; use already validated board power.

## Firmware behavior

This experiment preserves M7's newline-delimited ASCII CDC commands:
`INFO`, `STATUS`, `MEASURE`, `PING`, `HELP`. `INFO` identifies firmware
`m8_usb_cdc_measure`, protocol `3`.

`MEASURE` reads the INA226 manufacturer/die ID (`0x5449`/`0x2260`), configures
`0x4527` (same config used in experiment 11), and reads the raw BUS (0x02)
and SHUNT (0x01) registers. It converts with:

- `bus_voltage_mV ≈ bus_raw × 1.25` (nearest whole mV).
- `current_mA ≈ shunt_raw_signed × 0.25` (nearest whole mA, **R010 only**).

The Current/Power INA226 calibration registers are **not** used: conversion
uses only raw shunt/bus registers. Current is positive when flow is from IN+
to IN− and may be negative if reversed. Current output is rounded to whole
milliampere; for finer resolution see experiment 11's shunt raw displays.

Typical real-measurement response (example only; not yet measured on M8):

```
MEASURE status=ok voltage_mV=6965 current_mA=69
```

Missing sensor, wrong identity, NACK, bus timeout, failed configuration, or
failed register read returns **no numeric readings**:

```
MEASURE status=error reason=sensor_io voltage_mV=NA current_mA=NA
```

M8 does NOT request PPS power, drive CC, or modify PD negotiation.

## Installation and build (Windows 11)

Download the package and `Install-M8-Only.ps1` into `C:\pj\pd_pps`.
Run the installer, then from `C:\pj\pd_pps\src\pd_pps`:

```powershell
$Project = 'firmware/experiments/21_usb_cdc_ina226'
.\dev-workspace\pd-pps.ps1 -Action Build -Project $Project
# Only after successful Build and wiring checks:
.\dev-workspace\pd-pps.ps1 -Action Flash -Project $Project
```

Reconnect PC USB data (no PD J1), confirm the COM port, then run:

```powershell
.\firmware\experiments\21_usb_cdc_ina226\Test-M8-CdcMeasure.ps1 -Port COM9
```

The test accepts either correct `status=error reason=sensor_io` (not yet
wired) or the `status=ok` format when a recognized INA226 is wired. A
PASS **does not** establish voltage/current accuracy. Verify measured values
separately against a DMM and a known load before product integration.

## Offline host regression tests

```bash
cd firmware/experiments/21_usb_cdc_ina226
gcc -std=c11 -Wall -Wextra -Werror -I. host_test/test_measure.c -o /tmp/m8_cdc_test
/tmp/m8_cdc_test
gcc -std=c11 -Wall -Wextra -Werror -I. host_test/test_backend.c m8_measurement.c -o /tmp/m8_backend_test
/tmp/m8_backend_test
gcc -std=c11 -Wall -Wextra -Werror -I. host_test/test_ina226.c -o /tmp/m8_ina226_test
/tmp/m8_ina226_test
```

## Status and change boundary

Host tests validate conversions and error handling; they do not establish
measurement accuracy. On 2026-11-09, Windows 11 Build passed (FLASH 5,476 B;
RAM 1,000 B) after replacing the invalid external `Delay_Us` declaration
with the macro from `ch32fun.h`. M8 identified itself on COM9 as
`m8_usb_cdc_measure protocol=3` and returned
`MEASURE status=ok voltage_mV=49 current_mA=0`. The test passed 20 PING
round-trips and 10 additional consistent MEASURE responses.
This establishes real INA226 communication, NOT DMM-verified accuracy,
known-load current measurement, or PD/PPS operation alongside PC USB.
See the wiring reference and M8 verification record above.
Do not change/commit experiments 11, 16b, 18, 19, 20, ch32fun,
ROADMAP or other user work. No automatic Git commit or push.
