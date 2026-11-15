# Experiment 28 — PD17 + USB CDC read-only integration (M5 hardware candidate)

**2026-11-13 / CH32X035F8U6 / exploratory prototype, not production firmware.**

This project is an **independent copy** of the pinned `17_pd_spr_sink_rebuild` sources, extended with M5-1/M5-2/M5-4 read-only command handling and USBFS CDC. The assembly script generates the local `src/main.c`, `src/pd_sensors.c`, `src/pd_sensors.h` from Git-tracked PD17 sources **without modifying PD17**. No live M28 hardware result is claimed until Windows Build/Flash and bench logs are supplied.

## Functionality implemented in source

- PD17's normal CC/PHY/Policy Engine still runs first in each loop; USB service handles **at most one** RX byte or EP3 submission after the PD work.
- USBFS IRQ and USBPD IRQ are separate. USBFS IRQ reports bus reset, EP2 OUT, EP3 IN completion into a fixed-size ISR transport; foreground code consumes it.
- ASCII LF-delimited **protocol 7 (draft)**: `INFO`, `HEALTH`, `GET_CAPS`, `GET_CAP 1`–`GET_CAP 7`, `STATUS`, `MEASURE`. Write commands rejected (`ERR code=read_only`). USB endpoint replies **<=63 bytes including LF**.
- `GET_CAPS`, `GET_CAP`, `STATUS` read **real in-memory PD17 `pe_t`**, not fixtures. They expose no confirmed contract during negotiation, hard reset, recovery or detach.
- INA226 `MEASURE` is based on **PD17's one-shot READY measurement**, not periodic sampling. Report `sensor_unavailable` before it is sampled, if the capture failed, after its **2s freshness window**, or after PD state/contract changes. Do not imply continuously updated data.
- EP3 in-flight submission timeout **2500ms**: physically disable USBFS port and D+ pullup; wait **150ms non-blockingly** while PD17 continues; re-run `USBFSSetup()` from foreground. `USBFSSetup()` itself contains a short ~1ms delay, which still requires real timing validation.
- On USB bus RESET or RX overflow: forget old commands/replies. No automatic PD output settings, no source renegotiation initiated by USB.
- CDC ACM uses experimental test descriptors `VID_1209`, `PID_D035`, `bcdDevice 0x0008`, `serial 008`. **Do not use these identifiers for a product without permission/assignment.**

## Tests and current qualification

- Linux GCC host: M28 runtime/transport assertions **19/19 PASS** (the non-hardware M5 session and timeout event loop). Source structural tests are separate.
- CH32X035 Build **not yet established** (must run Windows xPack build).
- USBPD+USBFS concurrent real-hardware operation **not yet established**.
- USB-C J1 PD and added PC USB-C share D+/D− nets on this evaluation wiring; the user has authorized a simultaneous-connection trial, **but authorization is not electrical compatibility evidence**. Review whether the PD source has any active USB 2.0 data interface, VBUS paths and backfeeding before the combined test. A charger with active D lines connected to a PC host can create bus contention; do not treat all charger/PC combinations as safe.
- USB VBUS detect, host disconnect gating, current limiting/ESD/reverse-power protection remain product design gates.

## Installation / tests (Windows 11)

Run the standalone installer from the ZIP root. It copies source from the **exact pinned PD17 Git state** into experiment28. User changes elsewhere are left untouched.

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
.\firmware\experiments\28_pd_usb_cdc_integrated\Test-M28-Host.ps1
.\firmware\experiments\28_pd_usb_cdc_integrated\Build-M28.ps1
```

After an explicitly authorized Flash and electrical review, run `Test-M28-Cdc.ps1 -Port COMx` to query only read-only commands. `-Port` may be omitted only when exactly one M28 serial `008` is detected.

The first command runs static assertions and, where available, native-C host tests; on systems without native GCC it explicitly reports runtime tests skipped and performs an RISC-V portable C syntax check if the configured compiler is present. The second command only **Builds**; it never flashes.

If the Build completes, inspect **Flash/RAM** usage and interrupt vector names before any device Flash. These tests do not certify real-time PD processing, coexistence of two USB ports, VBUS/backfeed behavior, or USB 2.0 compliance.

## After successful Build: staged physical acceptance (not automated)

1. Record `Build` output and sizes. Compare with CH32X035 flash/SRAM budget and prior PD17 and M8F.
2. Verify isolated bench power, ground, current limit, cable D+/D− topology, WCH-LinkE, and ability to restore PD17. No load, no forced fault injection.
3. First test CDC by itself, identifying `firmware=m28_pd_usb_cdc protocol=7` and the new `008` serial number. Expect `STATUS state=detached ...` and no stale `MEASURE` values.
4. Under explicitly verified wiring, introduce the chosen PD source with no load/current-limited test setup. Confirm `GET_CAPS`/`GET_CAP`/`STATUS` reflect real PD17 contract; compare PD17 normal-operation diagnostics.
5. Test USB cable removal/reinsertion, then controlled latency, PPS refresh interval, USB watchdog diagnostics, I2C/ADC timing and 30+min soak in a later phase. Revert on any unexpected voltage, USB errors, PD drops or thermal issue.

**Do not Flash automatically.** Experiment 28 does not implement `SET_PPS` and is not a substitute for protection hardware.
