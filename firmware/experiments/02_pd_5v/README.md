# 02_pps_5v -- USB-C VBUS 5V initial test

Target: WeAct CH32X035F8U6 + Ubuntu 24.04 + WCH-LinkE + ch32fun.

## Purpose and limitations

This code **does not request a USB PD/PPS contract** and does not generate 5V.
A compatible USB-C charger provides default 5V VBUS after detecting the
sink's CC termination (Rd). The code blinks the onboard LED (PB12) three
times per cycle to confirm firmware execution. The 5V measurement is made
externally with a multimeter.

Do not use this as evidence that 5V is present: LED blinking only confirms
that the firmware is running.

## Directory layout

    ~/pj/pps/
    ├── ch32fun/
    ├── 01_led_blink/
    └── 02_pps_5v/
        ├── pps_5v.c
        ├── funconfig.h
        └── Makefile

## Build and flash

    cd ~/pj/pps/02_pps_5v
    make build
    make flash

## Electrical test

1. Before connecting the charger, disconnect WCH-LinkE target 3V3 and
   turn off any other external board supply. Do not supply 3V3 and USB-C
   VBUS simultaneously until the board's power-supply topology has been
   checked. You may fully disconnect WCH-LinkE after flashing.
2. Connect the Daiso charger's USB-C output to the WeAct board USB-C input
   using a USB-C-to-USB-C cable. Leave the load disconnected.
3. Measure voltage between board VBUS (+) and GND (-): approximately 5V
   is expected once a valid Type-C sink is detected.
4. Confirm three quick onboard LED flashes and then a 500 ms pause.

## Not in this step

* Parsing Source_Capabilities
* Requesting a 5V fixed PDO
* PPS programmable voltage/current requests
* Current limiting, load switching, VBUS presence monitoring

For explicit PD negotiations, start from the existing upstream example
`../ch32fun/examples_x035/usbpd_sink` and verify its exact code and
configuration for this board before enabling load power.
