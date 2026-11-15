# 09_pps_adc_voltage

Based on user-reported working `08_pps_manual_voltage_current_reconnect_v2`.
ADC source is user-tested standalone CH32X035 ADC_IN0 PA0 firmware, but **combined firmware is untested**.

Hardware: CH32X035F8U6, PA0 (IC pin 2), 100kΩ from VBUS to PA0, 10kΩ from PA0 to common GND. No direct VBUS-to-PA0 connection.
The 3.3V ADC reference is assumed and voltage measurement is not calibrated. Existing DMM comparison ~5.12V vs ADC ~5.15V.

Commands: `make`, `make test`, `make flash`; terminal:
`python3 ../minichlink_local_echo.py ../ch32fun/minichlink/minichlink -T`

`status` / `s` now prints measured ADC value and VBUS. It only prints target delta in ready/PS_RDY state. Values must not be interpreted as actual measured load current.

Firmware starts with prior default 7000mV / 1000mA. **This may request 7V automatically.** Before flashing ensure power path, supply and connected loads are suitable for >5V; otherwise modify `INITIAL_VOLTAGE_MV` to 5000 (when valid for selected APDO) or use a known safe supply. No higher-voltage tests until board regulator path and overvoltage protection are verified.

ADC acquisition is only done when `status` is run, to reduce disruption of timing-sensitive PD handling. No ADC measurement inside IRQ or refresh loop.
