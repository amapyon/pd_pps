# 10_pps_adc_calibrated

Based on hardware-verified `09_pps_adc_voltage`. PPS control, CLI, USB PD stack,
startup target (7000mV/1000mA), and ADC acquisition routines are unchanged.

## Changes

Only `status` display gains both uncalibrated and calibrated VBUS readings and
both requested-target differences. Calibration mathematics live in
`adc_calibration.c/h`; test in `tests/test_adc_calibration.c`.

Calibration input data (PPS target / DMM VBUS / mean ADC raw):

| Target | DMM | ADC |
|--:|--:|--:|
| 4V | 4.00V | 457 |
| 5V | 5.01V | 569 |
| 6V | 6.02V | 679.5 |
| 7V | 7.03V | 790.5 |
| 8V | 8.03V | 900.5 |
| 9V | 9.04V | 1012 |

Least-squares: VBUS[mV] = 9.08901162*raw - 156.48462433.
Fixed point implementation: `((raw*9089012 - 156484624) + 500000)/1000000`
for positive numerator, else 0.

**Validity**: empirically verified only over DMM 4.00..9.04V, raw 457..1012.
Values outside are **extrapolations** and a warning prints. There is NO
safety shutdown and NO clamp of the USB PD requested voltage.

**Safety**: Divider 100k from VBUS to PA0 (ADC_IN0, chip physical pin 2),
10k from PA0 to GND; common reference GND. Never connect VBUS directly to PA0.
Before requesting higher PPS voltage, verify the board's power path and all
attached components' rated voltages. USB PD requested current is not measured.

Commands from project root (`~/pj/pps`):
```
cd 10_pps_adc_calibrated
make test
make
make flash
python3 ../minichlink_local_echo.py ../ch32fun/minichlink/minichlink -T
```

Independent validation on **non-calibration** points: set 4.50, 6.50, 8.50 V
(using `4500`, `6500`, `8500`), then `s` and DMM VBUS reading at each point.
Only experiment within confirmed safe hardware voltage ratings.
