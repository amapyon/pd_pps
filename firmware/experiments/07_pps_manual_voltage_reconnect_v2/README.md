# 07_pps_manual_voltage_reconnect_v2

## Purpose
Fix the observed failure where first PD attach succeeds but after a cable replug the sink gets stuck at `Cable Detected` and times out repeatedly.

Based on the previous reconnect firmware, this release adds a sink-initiated **Get_Source_Cap** control message while waiting for Source_Capabilities. The approach is informed by Linux TCPM, which probes explicitly on Source_Capabilities timeout. This is a **hypothesis-driven fix, not verified on the CH32X035 hardware**.

## Changed behavior
- Upon entering `Cable Detected`, wait 1.2 s for an unsolicited Source_Capabilities message.
- If none arrives, send `Get_Source_Cap`; repeat at 3 s intervals, maximum 4 attempts.
- Continue the original 20 s negotiation timeout and retry logic.
- Retain the last requested PPS voltage across reconnection.
- Keep the original no-echo firmware handler; use the Ubuntu local echo Python wrapper.
- Each probe produces `PD: Get_Source_Cap sent (n/4)` to help debug whether the source responds.

## Installation (parallel directory)

```bash
cd ~/pj/pps
unzip ~/Downloads/07_pps_manual_voltage_reconnect_v2.zip -d .
cd 07_pps_manual_voltage_reconnect_v2
make test
make build
# Disconnect charger, leave your confirmed WCH-LinkE power arrangement unchanged.
make flash
python3 ../minichlink_local_echo.py ../ch32fun/minichlink/minichlink -T
```

## Test
1. Start console with the USB-C charger unplugged.
2. Insert the charger; confirm PPS enters 7.00V and Source Capabilities are printed.
3. Enter `7.02`; confirm the requested voltage changes to 7020mV.
4. Unplug/replug only the charger; leave WCH-LinkE powered.
5. Look for `PD: Get_Source_Cap sent (1/4)`, `PD: negotiate=OK`, and `PPS: restore PS_RDY`.
6. Test a second replug. If it remains stuck in `Cable Detected`, save the entire console log.

## Limitations
- Because the underlying PD library does not implement a full sink policy engine, an explicit capability request may not recover every charger. The charger must still be physically attached.
- If the source never sends Source_Capabilities and doesn't respond to Get_Source_Cap, this firmware retries but cannot guarantee recovery.
- A sent request means only that the CH32X035 transmission was started; it does **not** prove the source acknowledged it.
- No load resistors until reconnection is verified; use FNB-C2 to confirm actual voltage.

## MCU pin pair notation (QFN20)
- CC1: PC14, physical pin 19
- CC2: PC15, physical pin 20
- Debug DIO: PC18, physical pin 14
- Debug DCK: PC19, physical pin 16
- LED: PB12, physical pin 15
