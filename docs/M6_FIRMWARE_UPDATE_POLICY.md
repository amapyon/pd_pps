# M6 Firmware Update Policy: WCH-LinkE Only

- Decision: 2026-11-22
- Status: product interface policy DECIDED; recovery acceptance testing NOT_VERIFIED
- Canonical product requirements: [PRODUCT_SPEC.md](PRODUCT_SPEC.md)

## Decided interfaces

| Interface | Product role | Firmware programming |
|---|---|---|
| USB-C #1 (PD/PPS) | PD/PPS input and CC negotiation | No |
| USB-C #2 (PC) | USB CDC-ACM commands, status/capability/measurement retrieval and logging, and PPS setting commands with responses | **No** |
| SWD pads/connector with external WCH-LinkE | Firmware programming, debugging, and recovery | **Only adopted method** |

- No PC USB-C #2 firmware-update function will be shipped: no CDC firmware-transfer/update/BOOT command, no USB ISP update workflow, and no USB MSC/UF2 drag-and-drop updater. This does **not** prohibit normal CDC PPS setting commands; they remain subject to safety validation.
- WCH-LinkE connects to the PC, and programming targets the CH32X035 over SWD. Connector/pad design, customer programming fixture and whether the probe ships with a product remain undecided.
- This choice does **not** guarantee that installing no drivers is required on all PCs. USB CDC and the debugger have separate host-driver requirements.

## History, not product scope

- 2026-11-22: WCH-LinkE SWD probe identified CH32X035 and a 62 KiB Flash device; this establishes a debug identity path, not a recovery test.
- 2026-11-22: Download Key/K1 yielded USB ROM ISP candidate VID:PID 1A86:55E0, Windows Code 28 (driver missing). ROM ISP chip interrogation, writing, and restoration were not done. The historical experiment is preserved, **not adopted in the product**.
- The M6 experimental firmware `m6_usb_isp_entry` (BOOT transition disabled, build-only verified) and its logs remain historical artifacts; they are not deleted, flashed, or merged as part of this decision.

## M6 acceptance gates remaining open

1. SWD accessibility in the final PCB, power isolation, grounds and anti-backfeed conditions.
2. Explicit target identity, image/board-version verification and separately confirmed Flash authorization.
3. Controlled programming and image verification without damaging protected reference firmware.
4. Failure-mode recovery test using WCH-LinkE; successful identification alone does not prove recovery.
5. Instructions for assembled products and bare PCBs (connector, fixture, compatible tools and images).

Firmware, hardware, Git history, and diagnostic raw data are not changed by this documentation-only update.
