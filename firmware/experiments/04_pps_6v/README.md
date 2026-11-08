# 04_pps_6v

WeAct CH32X035F8U6 / Ubuntu 24.04 / WCH-LinkE / ch32fun.

## Purpose

Request **6.00 V** using the USB PD PPS APDO, not a fixed PDO. Intended
for the DAISO 20 W PD/PPS charger (JAN 4550746010529). Check the actual
Source Capabilities before requesting power.

## Directory layout

    ~/pj/pps/ch32fun/
    ~/pj/pps/04_pps_6v/pps_6v.c
    ~/pj/pps/04_pps_6v/funconfig.h
    ~/pj/pps/04_pps_6v/Makefile

The existing ch32fun checkout must contain
`examples_x035/usbpd_sink/usbpd.h`.

## Build and program

    cd ~/pj/pps/04_pps_6v
    make clean
    make build
    make flash

You can monitor debug output using WCH-LinkE (`minichlink -T`).
Avoid tying the programmer's 3.3 V output to the USB-C-powered board's
3.3 V rail unless the exact power circuit has been verified.

## Expected log excerpts

    PD: source PDO count=4
    PD: PDO[3] PPS 5000-11000 mV, max 1800 mA
    PPS: request 6000mV, PDO index=3, API voltage=60 (100mV units)
    PPS: selection returned, state=...
    PPS: refresh 6000mV, state=...

Expected VBUS when supported and negotiated: ~6 V. Verify with a
meter, initially with no downstream load. The LED pattern does not
measure or prove the voltage.

## LED states

- Three 200 ms flashes, once every 3 s: PPS selection call issued.
- Two 300 ms flashes, then 2.5 s pause: no matching PPS APDO.
- Five 300 ms flashes, then 2.5 s pause: init/negotiation error.

## Important limitations

- `USBPD_SelectPDO()` is based on the user-supplied `usbpd_sink.c` example.
  Its internal implementation (in usbpd.h) was not available for review here.
- In particular, check its PPS voltage unit and PPS request/response handling
  against your local `usbpd.h` before relying on the firmware.
- `USBPD_SelectPDO()` exposes no desired current parameter in the example.
  The 500 mA condition in the source only verifies that the advertised
  source capability is at least 500 mA; it does NOT set a 500 mA limit.
- A PPS request is refreshed every 5 s, but reconnect / cable disconnect
  and invalid-contract recovery are not implemented yet.
- The program does NOT measure VBUS. The USB PD meter must confirm 6.0 V.
- Do not connect 6 V-sensitive equipment until validated.
