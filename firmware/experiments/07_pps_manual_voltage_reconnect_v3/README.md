# 07_pps_manual_voltage_reconnect_v3 (diagnostic firmware)

Compared with the user-confirmed v2, this version:

- Prints **every** Source PDO (raw hexadecimal and decoded fixed/PPS fields) on negotiation.
- Adds console command `caps` or `c` to display the latest saved capabilities.
- If the library enters `Got Source Capabilities` again while the device is ready, negotiates a new contract **without USBPD_Reset()**, instead of immediately starting a reset loop.
- Keeps the last requested voltage and Ubuntu-side local echo workflow.
- If there is no compatible PPS APDO, requests fixed PDO 0 (normally 5 V) and does not force PPS.

The 5-PDO advertisement previously observed with the FNB-C2 **is not yet explained**. The patch will not make a charger supply PPS if its actual Source_Capabilities lacks an applicable PPS APDO. Do not connect an external load until stable operation is established.

## Build on user's Ubuntu machine

```sh
cd ~/pj/pps
unzip ~/Downloads/07_pps_manual_voltage_reconnect_v3.zip -d .
cd 07_pps_manual_voltage_reconnect_v3
make test
make clean
make build
# Unplug charger prior to writing
make flash
python3 ../minichlink_local_echo.py ../ch32fun/minichlink/minichlink -T
```

Try `caps` immediately after the connection has been established (whether PPS is supported or not) and share the full PDO list. Compare UGREEN direct with UGREEN through the FNB-C2 in passive measurement mode, using the same USB-C port and cable and no other charger loads.

## Verification

Host-side parsing and CC filter tests run successfully. CH32X035 cross-compile and hardware operation were NOT executed by the assistant. Keep the v2 directory as the known-good rollback.

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
