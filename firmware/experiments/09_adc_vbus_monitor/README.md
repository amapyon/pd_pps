# CH32X035 VBUS monitor (standalone)

Based on the previously tested `09_adc_vbus_monitor`; ADC input: PA0 (IC physical pin 2), ADC_IN0.

## Wiring

Only after confirming the WeAct board VBUS/GND routing:

- VBUS -> 100 kOhm (top resistor) -> measurement node -> 10 kOhm (bottom resistor) -> GND
- measurement node -> PA0 (physical pin 2)
- Common GND is necessary. Never connect VBUS directly to PA0.

## Build

Unzip in `~/pj/pps` so `09_adc_vbus_monitor` and `ch32fun` are siblings.

```sh
cd ~/pj/pps/09_adc_vbus_monitor
make
make flash
python3 ../minichlink_local_echo.py ../ch32fun/minichlink/minichlink -T
```

## Conversion

- ADC nominal reference: 3300 mV (assumed, not measured).
- Divider: 100k/10k => VBUS = V_PA0 * 11 (nominal).
- Uses unrounded raw ADC count when calculating VBUS and `uint64_t` intermediates.
- No offset/gain calibration implemented.
- Validate against DMM. Previous 5.12V check yielded about 5.16V ADC-derived.
- Do not raise VBUS above the previously tested ~5V until the board power path and protection have been confirmed.
