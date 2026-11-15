# M35 USBFS PHY isolation diagnostic (temporary hardware-only test)

## Purpose

Compare the PC16/PC17 USB data pin voltages while USB PHY is active vs
firmware-disabled **without changing the PD charger connection**. This is
not a USB CDC fix, and not intended for normal operation.

The WeActStudio CH32X035 CoreBoard V1.0 schematic connects its original
USB-C connector's D+/D- directly to the `USB_DP`/`USB_DN` nets and TP1/TP2,
respectively, routed to MCU PC17/PC16. If an additional PC-facing USB-C port
is wired to those same nets, both connectors share data lines unless
physical isolation exists. Confirm continuity **with all cables and power
removed** before testing. Do not connect PC and PD charger data lines to
those nets simultaneously until the path is positively verified safe.

## Critical restrictions

- **DO NOT attach the PC USB data cable during the PD test.**
- Use a known-good charger, low-risk load, and stable VDD33 power; watch VBUS,
  MCU temperature, and PD contract. Stop on abnormal readings.
- Do not short D+/D- or measure resistance while powered.
- Maintain power to MCU across PD detach if RAM diagnostics will be read via CDC.
  If VDD33 falls on PD detach, RAM snapshots are lost; the LED and DMM
  comparison are still useful.
- Keep M34 BIN for rollback. M35 intentionally disables USBFS after 30 s of
  PD attach. It might interrupt USB enumeration; this is expected.

## Program behavior

- Start with M35 flashed, PD and PC USB unplugged; maintain safe board power.
- Connect only the PD charger (no PC-facing USB). For the first 30 seconds
  after firmware reports CC attach, USBFS remains ON; PB12 toggles every
  250ms (~2Hz blinking). Read D+ and D- with a DMM in this interval.
- After ~30 seconds the firmware disables USBFS port and interrupts, removes
  USB muxing and pullups, disables USB-internal pulldowns, and sets
  PC16/17 to floating inputs. PB12 toggles
  every 100ms (~5Hz blinking). Read D+ and D- again, with charger still on.
- The OFF state remains while PD is attached. Do not connect a PC data cable.
- When PD is disconnected, wait >0.5 s: USBFS is re-initialized.
  If board VDD is maintained, PC cable can now be connected to read RAM
  snapshots via `PHYDIAG`; only then reconnect the PC data cable.

## Test and build (Windows PowerShell from repo root)

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\35_pd_usbfs_phy_isolation\Test-M35-Host.ps1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\35_pd_usbfs_phy_isolation\Build-M35.ps1"
```

## Flash only after build succeeds

Unplug PC data and PD charger; use the established WCH-LinkE power setup.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\dev-workspace\pd-pps.ps1" -Action Flash -Project "firmware\experiments\35_pd_usbfs_phy_isolation"
```

## Read snapshots (only after charger detached and CDC recovered)

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\35_pd_usbfs_phy_isolation\Read-M35-Diag.ps1" -Port COM10
```

`PHYDIAG` = number of samples, stage (0:wait,1:baseline,2:off,3:restore,4:done).
`PS` = phase/time/PD/USB controller state; `PR` = AFIO/GPIOC registers.
Expected phases: A (attach), B (just before off), O (after off), D (detach),
R (after USBFS restored). AFIO bit 7 USB_IOEN should clear in O; UDP_PUE
bits 3:2 should also clear. GPIOC CFGXR low byte should become 0x44.

## Interpretation

- D+/D- stay near 1.1V in PHY OFF: likely driven/loaded externally, e.g.
  the original Type-C charger data wiring; check continuity and charger BC
  signaling. Not proof of which external driver.
- D+/D- change significantly in PHY OFF: MCU-side PHY/pullups contribute,
  but this alone does not prove a firmware defect.
- Do not infer USB signal shape or protocol from DMM DC voltages alone.

The PD policy and USB automatic recovery conditions are unmodified, except
M35 deliberately gates USB service while PHY is disabled and calls USBFSSetup
once after PD detach. No RISC-V target build or hardware validation was
available in the package authoring environment.
