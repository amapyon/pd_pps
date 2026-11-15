# CH32X035 ADC standalone test

Target: WeAct CH32X035F8U6, PA0 / physical IC pin 2 / ADC_IN0.

This is an initial **unverified** standalone firmware. Build and hardware tests must be done on your Ubuntu machine. It does not negotiate USB PD or control PPS.

**Do not connect USB PD VBUS directly to PA0.** Test using a known safe 0-3.3V analog source referenced to board GND; ensure ADC pin voltage never exceeds the MCU VDD.

Copy folder to `~/pj/pps/09_adc_standalone` next to existing `ch32fun/`.

Build with `make PREFIX=riscv64-unknown-elf`. On your ch32fun configuration this might work as `make` alone.

The printed millivolts assume MCU VDD=3300mV; change ASSUMED_VDD_MV after measuring MCU VDD. No PPS VBUS conversion is implemented yet.

Do not flash until the board pin mapping has been confirmed against the schematic.
