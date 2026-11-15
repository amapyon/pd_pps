# 11_pps_ina226_monitor (CH32X035F8U6, Windows 11)

Based on **10_pps_adc_calibrated** from the user-supplied `pd_pps-main.zip` (2026-10-23 snapshot). It preserves its existing USB PD/PPS transport, reconnect policy, commands, and the PA0 ADC calibration, while adding an INA226 voltage/current/power monitor with an **R010 = 0.010 ohm shunt**.

## IMPORTANT: hardware and electrical limits

- The INA226 is powered by **3.3 V only**, never by PPS VBUS (which can exceed 5 V). Digital SCL/SDA pull-ups must also go to **3.3 V**, NOT VBUS.
- Power stage: USB PD source/board VBUS -> **R010 high-current IN+ side** -> R010 -> **high-current IN- side** -> **external load positive**. External load negative connects to common GND. The sensor's **VBS/VBUS** voltage input connects to the **load-side IN- node** (unless already connected on the module PCB; check with a multimeter). Connect the INA226's sense IN+/IN- to respective R010 ends only when the particular board does not already do this; check continuity first.
- This wiring measures the **external load branch current**, not the entire USB-C source current, and not the CH32X035 board's own consumption. Measuring the board's own current requires a different series insertion point.
- The module variants and PCB revisions may have different positions for the large current pads. Verify the actual **R010 marked resistor, polarity, pin labels, and copper continuity** before powering it. Never route load current via the signal-only header pins unless their conductor ratings are verified.
- Confirm shunt power rating and pad/wire current rating from actual hardware; 0.01 ohm itself does **not** guarantee 8A continuous current capability. Connect a fuse/current-limited load for initial testing.
- Existing firmware still starts with a **7.00 V / 1.00 A PPS REQUEST** when a compatible source is detected. This is *not* a regulated 1A output command; it advertises requested current limit. Before flashing make sure all connected loads and hardware paths are rated for 7V; or set `INITIAL_VOLTAGE_MV` to `5000u` in `pps_manual_voltage.c` and rebuild.
- The INA226 monitoring is **informational only**. This firmware does not implement hardware overvoltage/overcurrent cut-off or closed-loop PPS control. If measurements fail, PD PPS continues operating.

## Connections (CH32X035F8U6 QFN20)

| CoreBoard / CH32X035 | INA226 module | Notes |
|---|---|---|
| PA2 (MCU pin 4) | SCL | software I2C, emulated open drain, pull-up to 3.3V |
| PA3 (MCU pin 5) | SDA | software I2C, emulated open drain, pull-up to 3.3V |
| 3.3V regulated rail | VCC | verify 3.3V at the actual header before connecting |
| GND | GND | common reference with external load |
| VBUS external output | high-current pad on R010 IN+ side | the board's ADC divider on PA0 remains in place |
| high-current pad on R010 IN- side | external load positive | load-side bus measurement |
| load-side IN- node | VBS / VBUS | check whether the module already has this connection |
| external load negative | GND | do not place R010 in ground for this design |
| (none) | ALERT / ALE | optional, unused |

WCH-LinkE remains on PC18/PC19, USB PD CC1/CC2 on PC14/PC15, existing ADC on PA0, LED on PB12. All are unchanged. Some online examples use PA10/PA11 for hardware I2C, but those pads are **not available on the QFN20 part**. We avoid PC16/17 as they also serve USB D-/D+.

See `../ina226_wiring.svg`, `../ina226_wiring.png` or `../ina226_wiring.pdf` in the downloadable bundle. The schematic is functional (not a verified physical PCB footprint).

## Measurement implementation

- 7-bit I2C address scan `0x40`-`0x4F`; verify manufacturer ID `0x5449`, die ID `0x226x` before configuring.
- Software I2C on PA2/PA3, **emulated open drain** (floating input for HIGH, push-pull LOW for LOW), 5us delay per half-period plus GPIO mode-switch overhead (frequency below nominal 100kHz). Clock stretching and stuck-line checks have bounded waits. No calls from an interrupt handler.
- Config register `0x4527`: 16 averages, bus/shunt conversion nominal 1.1ms each, continuous conversions. After first setup allow about 40ms for the averaged reading to settle.
- INA226 bus-voltage register LSB = 1.25mV (converted to mV).
- INA226 shunt-voltage register signed LSB = 2.5uV; R010 => 250uA per count. Firmware calculates current from this raw register, **without using** its Current/Power registers (which require a user Calibration register setting).
- Power is calculated as `bus_voltage * current` in integer fixed-point units. Reverse current yields a negative sign.
- Samples are read only on `status/s`, `meter/m` or existing status call sites, not inside the USB PD ISR or PPS Keep Alive path.
- Existing PA0 voltage calibration is unchanged and still only validated in the raw 457..1012 range (DMM 4.00..9.04 V).

## New commands

| Input | Action |
|---|---|
| `m` or `meter` | INA226 bus voltage, external load current, load power |
| `scan` | I2C probe 0x40..0x4F, ACK/NACK, Manufacturer/Die ID, CONFIG write/readback |
| `s` or `status` | previous PD/ADC output plus INA226 measurement |
| All previous commands | unchanged, including `v`, `i`, `set`, `limits`, `caps`, `retry`, `help` |

Example indicative response **only** (not a measured result):

```text
INA226: addr=0x40 BUS=7.000 V, I=500.000 mA, P=3.500 W
INA226: shunt_raw=2000 bus_raw=5600 (R010 0.010 ohm; measured load current)
```

Missing / disconnected INA226 displays `NOT FOUND/read failed`, and PPS continues as before.

## Windows 11: add to the current layout

Copy the folder `11_pps_ina226_monitor` into:

`C:\pj\pd_pps\src\pd_pps\11_pps_ina226_monitor`

The existing `C:\pj\pd_pps\pd-pps.ps1` and toolchain need no changes. In PowerShell:

```powershell
cd C:\pj\pd_pps
.\pd-pps.ps1 -Action Build -Project 11_pps_ina226_monitor
# Only after verifying wiring and safe load voltage/current:
.\pd-pps.ps1 -Action Flash -Project 11_pps_ina226_monitor
.\pd-pps.ps1 -Action Log
```

Use `m` in the console to read INA226. The current VS Code workspace lists projects explicitly; the downloaded bundle includes an **optional** replacement workspace file with project 11 added. Back up your `pd_pps.code-workspace` before replacing it. Alternatively, use the direct PowerShell commands above.

## Tests / verification status

- Source code and host C logic tests created in this environment: `make test` passes all 7 test executables. The original tests cover the existing parser, APDO, reconnect, PPS RDO, ADC calibration; one new test checks INA226 unit conversions.
- **Not performed:** CH32X035 cross-compile, writing real firmware, and on-board I2C/voltage/current validation. Please run `-Action Build` on Windows before flashing; resolve any target compiler errors first.
- At power-on, with 3.3V and I2C connected but **no high-current load**, `m` should respond without a fatal failure; voltage/current may be zero. Verify VBUS with a multimeter and a small known load before relying on the readings.

## References

- [Seller: CJMCU-226 INA226 R010/R100](https://electronicwork.shop/items/63ecfbd67c42ee776b674b2d)
- [Texas Instruments INA226 datasheet](https://www.ti.com/lit/ds/symlink/ina226.pdf)
- [WCH CH32X035/X033 datasheet](https://akizukidenshi.com/goodsaffix/CH32X035.pdf)
- [WeActStudio CH32X035 CoreBoard hardware](https://github.com/WeActStudio/WeActStudio.CH32X035CoreBoard)

## 2026-10-31: `scan` diagnostics update

The DMM confirmed INA226 VCC, SCL, SDA at ~3.3V (idle), and A0-GND / A1-GND at ~9.9kΩ; hence the **expected** 7-bit address is **0x40**, although ACK has not yet been observed on the bus.

In the WCH-LinkE debug terminal, enter `scan` followed by Enter. It prints idle pin levels, one result per address in the legal 0x40..0x4F range, and—when an address ACKs—reads the manufacturer register 0xFE and die register 0xFF, reporting the **failing I2C transaction stage** (START, SLA+W, register pointer, repeated START, SLA+R, first/second data byte). After *both IDs* match INA226, it writes the existing INA226 CONFIG 0x4527 and verifies readback. This is a diagnostic with an INA226 configuration side effect; it never changes PPS voltage/current, PDOs, or PA0 ADC configuration.

Interpretation:

- `0x40 NACK`, all others `NACK`: no I2C address response; inspect PA2/PA3 actual pin mapping, jumper wiring, signal activity with logic analyzer or oscilloscope. DMM steady High is **not** proof of clock pulses.
- `SCL stayed LOW` / `SDA stayed LOW`: bus electrical fault / stuck line; correct wiring or pullups before retry.
- `0x40 ACK` but `... ID read failed at ...`: locate the failing stage in repeated-START read sequence.
- `0x40 ACK` with incorrect IDs: not a verified INA226 at that address (or transaction data corrupt).
- `0x40 ... CONFIG=0x4527 OK; INA226 READY`: communication and setup succeeded. Run `m` to read values.

**Scope:** This update has only been host-tested here; Windows CH32X035 cross-build and on-board I2C have not been verified. Preserve the previous firmware and keep PPS loads disconnected for early bring-up. For the `scan` check, power the module using only 3.3V/GND plus SDA/SCL; ALE and VBS can remain disconnected while testing I2C identification.

## 2026-10-31: CH32X035 GPIO open-drain correction

An actual device scan returned NACK for all addresses, even with the INA226 VCC/SCL/SDA at 3.3V and the A0/A1 pads pulled to GND (~9.9kOhm each). The previous code configured PA2/PA3 as `GPIO_CNF_OUT_OD` and used GPIO BSHR to release the line. CH32X035 has a documented GPIO limitation: general-purpose output open-drain does not actually release the pad. When an INA226 pulls SDA LOW for ACK, the CH32X035 may continue driving SDA HIGH, preventing recognition.

Corrected implementation uses `GPIO_CNF_IN_FLOATING` to release the pin and `GPIO_Speed_2MHz | GPIO_CNF_OUT_PP` with a preloaded LOW latch to pull it down. External pull-ups to 3.3V are mandatory. No changes were made to USB PD/PPS, PA0 ADC, or commands. This is a targeted fix requiring an on-board re-test with `scan`, then `m`.

Community errata: https://github.com/ch32-riscv-ug/CH32X035#errata

For initial test, disconnect PPS high-current load and use only INA226 VCC/GND/SCL/SDA, with common ground. Flashing replaces MCU firmware; keep connected equipment rated for the programmed PPS request (currently starts at 7V when a PPS charger is attached).
