# 03_pd_9v — CH32X035F8U6 / 9 V Fixed PD Sink

## Purpose

Use the existing `ch32fun` library to negotiate USB PD, identify an advertised
**Fixed 9 V** Source PDO, and request it. No PPS requests are made.

## Directory

```text
~/pj/pps/
├── ch32fun/          # already cloned and configured
└── 03_pd_9v/
    ├── pd_9v.c
    ├── funconfig.h
    └── Makefile
```

## Requirements

- `../ch32fun/examples_x035/usbpd_sink/usbpd.h` exists.
- `../ch32fun/ch32fun/ch32fun.mk` exists.
- RISC-V GCC and WCH-LinkE/minichlink are already installed.
- The WeAct CoreBoard has a 3.3 V MCU supply.

## Build

```bash
cd ~/pj/pps/03_pd_9v
make clean
make build
```

## Program (without charger or any load attached)

```bash
make flash
```

## IMPORTANT: Before applying 9 V

1. Verify from the actual WeAct board's schematic/part numbers that its VBUS-
   fed 3.3 V regulator and all components connected to VBUS withstand 9 V.
2. The WCH-LinkE should not power the 3.3 V rail at the same time as the
   charger unless the specific board power paths have been confirmed.
3. Never apply 9 V to the MCU VDD/3V3 pin or any GPIO.
4. Apply the PD charger with no load and measure VBUS against GND using a meter.
5. Do not assume that LED confirmation means VBUS is 9 V.

## LED indication

- 3 quick blinks, then 500 ms off: Fixed 9 V PDO was found and the library's
  `USBPD_SelectPDO()` operation was invoked. **Not a voltage measurement.**
- 2 quick blinks: no usable 9 V Fixed PDO was found.
- 5 quick blinks: PD negotiation/init failed.

## Limitations

- This is based on the user's `usbpd_sink.c` from `ch32fun` and has not been
  compiled against their exact `usbpd.h`, nor tested on hardware.
- It checks for a 9 V PDO advertising at least 500 mA. The requested current
  is chosen by the ch32fun library's `USBPD_SelectPDO()` implementation;
  this program does **not** constrain the RDO to 500 mA.
- Automatic recovery after hot-unplug once 9 V is selected is not implemented;
  disconnecting/reconnecting may require resetting the board.
- The USB PD library may require a periodic service call depending on its
  implementation. Verify sustained VBUS in the actual hardware test.
