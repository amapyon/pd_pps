# M13a-1: PA0 ADC + INA226 passive sensing integration

## Status

- Input: uploaded `16b_source_caps_integrity(2).zip`, sensor sources reproduced without modifications except the local test header include path.
- Firmware basis: M13a safe-stop (builds on M12d). **Do not flash the earlier, obsolete M13a ZIP.**
- Purpose: collect **one** ADC/INA reading 300ms after first Ready, and one ADC reading on physical CC detach, with all text output deferred until after `PD17 TRACE END`.
- **NOT** a Hard Reset recovery implementation: existing `PE_RECOVERY` latched safe stop remains.
- Source: UGREEN 90551 USB-C1, other ports unused, PPS 7000mV requested 1000mA, no test load.

## Hardware

- ADC: PA0, VBUS→100kΩ→PA0→10kΩ→GND. Must be fitted as described; **never** direct-connect VBUS to PA0.
- ADC reference: assumed 3300mV, 12-bit, 8 conversions averaged. ADC division approximation used for `adc_uncal_mv`.
- Empirical 6-point ADC calibration usable only raw 457..1012 (~4..9V DMM). `adc_cal_mv=0` outside that range means **not available**, not zero volts.
- INA226: software-I2C PA2=SCL, PA3=SDA, 3.3V pull-ups, `0x40` observed (driver retains address scan 0x40..0x4f), R010=10mΩ. Shunt current depends on actual shunt wiring, NOT the requested PPS current.
- INA BUS voltage measures the sensor's BUS terminal, potentially not exactly PA0 VBUS; cannot substitute automatically for the PD safe-voltage decision.

## Timing and safety

- Sensor init at startup with USB-C Source disconnected. ADC calibration delays startup ~2ms, not part of PD AMS.
- ADC/INA read in main loop only when PE_READY/contract_valid and command==NONE, with 300ms delay from first Ready. Never in USBPD IRQ; **no print while connected**.
- I2C may block for the duration of a transaction, so only one measurement per CC connection is attempted in this bring-up. Further scheduling/latency characterization required before continuous monitoring.
- Sensor read failures do not alter PPS contract or initiate resets.
- On detach, sample PA0 once; printed after the existing deferred trace (trace_dropped count unaffected).
- Near-zero raw ADC counts and supply survival need hardware verification. Do not infer a vSafe0V transition from the detach-only sample: the CC detach happened ~150ms earlier.
- Do not intentionally induce Hard Reset or use inline USB power meters in this stage.

## Host tests and limitations

```
sh tests/run_m13a_sensors_host_test.sh
sh tests/run_m13a_host_test.sh
sh tests/run_m12b_host_test.sh
sh tests/run_m12c_host_test.sh
sh tests/run_m12d_host_test.sh
sh tests/run_host_test.sh
```

These validate scalar conversion, measurement scheduling logic, and pre-existing PHY/policy behavior. They **do not** execute the GPIO/ADC/I2C peripherals or reproduce PD electrical timing. Windows xPack cross-Build/Flash and attached sensor readings remain to be performed by the user.

## Windows test gate 1 (no USB-C Source)

Unplug the Source, stop Log and apply ZIP, Build, Flash, run Log. Check detached heartbeat. Do not attach the charger until this gate passes. Compare memory consumption to M12d flash 18596 B, RAM 2156 B.

## Windows test gate 2 (normal contract only)

Keep the WCH-LinkE; plug UGREEN USB-C1 for ~6 seconds then unplug. Read TRACE and additional lines:

```
PD17 SENSOR READY adc_ok=1 raw=... adc_uncal_mv=... cal_in_range=1 adc_cal_mv=... ina_ok=1 ina_bus_mv=... ina_current_ua=... ina_power_mw=...
PD17 SENSOR DETACHED adc_ok=1 raw=... adc_uncal_mv=...
```

Compare READY ADC/INA voltages with DMM if available. `ina_ok=0` is diagnostic (I2C/power connection) not a PD failure. `SENSOR DETACHED` can remain above zero due to discharge timing. Hard Reset recovery remains disabled until verified.
