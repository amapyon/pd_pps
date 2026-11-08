# 05_pps_sweep

CH32X035F8U6 (WeAct CoreBoard) / Ubuntu 24.04 / ch32fun / WCH-LinkE

## Function

Use a PPS APDO spanning 5.00 to 11.00 V. Request 5.00 V initially,
then step by 0.50 V every ~3 seconds up to 11.00 V and down to 5.00 V.
The endpoints are not repeated as additional steps, so one cycle is ~72 seconds.
Each change also refreshes the PPS request, at most 3 seconds apart during
normal operation.

The 04_pps_6v application must remain unchanged; this is a separate example.

## Directory layout

```
~/pj/pps/
├── ch32fun/
├── 04_pps_6v/
└── 05_pps_sweep/
    ├── pps_sweep.c
    ├── funconfig.h
    ├── Makefile
    └── README.md
```

## Commands

```
cd ~/pj/pps/05_pps_sweep
make clean
make build
# Disconnect the charger and all external loads before programming.
make flash
# If the target is safely powered and debug-connected:
../ch32fun/minichlink/minichlink -T
```

## Debug log

```
PPS: request 5000 mV, state=Power Supply Ready
PPS: request 5500 mV, state=Power Supply Ready
PPS: request 6000 mV, state=Power Supply Ready
...
PPS: request 11000 mV, state=Power Supply Ready
PPS: request 10500 mV, state=Power Supply Ready
```

## Status LED

- PB12 (IC physical pin 15): short 150ms pulse on each voltage request.
- 2 flashes + 2.5s pause: no PPS APDO spanning the sweep range.
- 5 flashes + 2.5s pause: PD init/negotiation failure.

These pulses indicate requests were issued, not measured VBUS or a validated
PD contract. Verify output using a USB PD meter.

## Safety and limitations

- Start with no external load; output may vary between 5V and 11V.
- Only VBUS is intended to carry the negotiated high voltage; never wire this
  directly to a 3.3V pin or 3.3V-only GPIO.
- Verify the WeAct board's voltage regulator / protection device ratings for
  continuous exposure to 11V before applying this firmware with a charger.
- Avoid backfeeding the board via WCH-LinkE 3.3V and USB-C simultaneously.
- Current is determined by the USB PD stack's RDO, not by the
  PPS_MIN_CURRENT_MA supply-capability check in this example.
- A disconnected charger / renegotiation error after initial connection is
  not automatically recovered; reset/reconnect as needed.
