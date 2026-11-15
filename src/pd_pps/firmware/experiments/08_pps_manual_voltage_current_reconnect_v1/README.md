# 08_pps_manual_voltage_current_reconnect_v1

Manual **PPS voltage + PPS requested operating current** for WeActStudio CH32X035F8U6 (QFN20), WCH-LinkE console, USB-C source (UGREEN 200W / DAISO 20W). Based on the previously user-tested `07_pps_manual_voltage_reconnect_v3`.

## Behavior retained from v3

- USB-C unplug/replug auto recovery, target restoration, CC detach debounce.
- PPS refresh every 3 seconds, including **both** desired voltage and requested current.
- Retained Source PDO dump (`caps`), 20-second negotiation timeout, bounded Get_Source_Cap retry, fixed-PDO fallback if PPS unavailable, PD state diagnostics.
- Local console echo is done by Ubuntu-side `minichlink_local_echo.py`, **not on the MCU**.
- Original `usbpd_current.h` is **unchanged**; it already implements 20mV/50mA PPS RDO encoding.

## Console commands

| Command | Effect | Example |
|---|---|---|
| `7000` or `7.00` | Set **voltage only**, preserving current | `7020` |
| `v <voltage>` or `voltage <voltage>` | Set voltage only | `v 8.00` / `v 8000` |
| `i <current>` or `current <current>` | Set **requested current limit only**, preserving voltage | `i 1.50` / `i 1500` |
| `set <voltage> <current>` | Set **both in one PD PPS Request RDO** | `set 8.00 1.50` |
| `<voltage> <current>` | Short form of `set` | `8000 1500` |
| `s` / `status` | Print both requested targets and PD state | `s` |
| `c` / `caps` | Dump advertised PDOs | `c` |
| `r` / `retry` | Restart PD session, retain BOTH targets | `r` |
| `?` / `help` | Command guide | `?` |

**Unit rules:** Values with a decimal point are volts / amps. A bare integer <= 100 is also volts / amps. Larger integers are mV / mA respectively. The supported **PPS request voltage** granularity is 20mV; **PPS request current** granularity is 50mA. Initial targets: **7000mV / 1000mA**. This program's local `USBPD_SelectPPS()` imposes a **minimum 1000mA** request and a maximum representable **6350mA**. The actual charger's PPS APDO imposes a tighter voltage/current limit, e.g. the previously observed DAISO 20W PPS maximum 1800mA and UGREEN 200W PPS maximum 3000mA. A current larger than the APDO is rejected, and the existing target is retained.

**IMPORTANT:** The **PPS requested current** is a negotiated maximum/operating-current request and is **not a commanded constant current**. With no external load, KWS-X1 might show `PPS 07.00V 01.50A` but `I:0.078A` remains about the same. It does NOT demonstrate current limiting. The actual charger current-limit behavior will be characterized later with appropriately mounted, cooled power resistors and a protected load connection, not through the WeAct logic header or breadboard rails.

## Build/flash on Ubuntu

Extract ZIP into `~/pj/pps` so project folder and existing `../ch32fun` are siblings:

```bash
cd ~/pj/pps
unzip ~/Downloads/08_pps_manual_voltage_current_reconnect_v1.zip
cd 08_pps_manual_voltage_current_reconnect_v1
make test
make clean
make build
# Disconnect the charger while flashing; preserve the proven WCH-LinkE wiring.
make flash
python3 ../minichlink_local_echo.py ../ch32fun/minichlink/minichlink -T
```

## No-load verification on KWS-X1

1. Connect to a PPS-capable charger with the **stable orientation** (UGREEN with FNB-C2 was orientation-dependent; KWS-X1 is another measurement path and needs its own check). Do not attach metal-clad resistors yet.
2. Confirm `PD: PPS ready` and `STATUS: desired=7000 mV, current limit=1000 mA`.
3. KWS-X1 shows `PPS 07.00V 01.00A`. Its **measured IBUS** might be ~0.08 A.
4. Send `i 1.50` -> after `PPS: set ... PS_RDY`, expect KWS-X1 `PPS 07.00V 01.50A`, with actual IBUS still near light-load value.
5. Send `v 8.00` -> expect `PPS 08.00V 01.50A`.
6. Send `set 7.02 1.00` -> expect `PPS 07.02V 01.00A`.
7. Send `i 3.05` on UGREEN with advertised max 3A -> expect console **error**, previous target unchanged.
8. Unplug/replug charger -> firmware automatically restores **both** the latest accepted target voltage and current limit.
9. If the KWS-X1 or an inline PD monitor interferes with negotiation, test with known-working direct connection. `PS_RDY` is not an independent VBUS/IBUS measurement.

## Pin list (CH32X035F8U6 QFN20)

- CC1: PC14, physical IC pin 19
- CC2: PC15, physical IC pin 20
- LED: PB12, physical IC pin 15
- WCH-LinkE DIO: PC18, physical IC pin 14
- WCH-LinkE DCK: PC19, physical IC pin 16

## Verification limitations

- Host-side unit tests in `tests/` are provided; **hardware behavior for this new version is unverified** until flashed and exercised on the actual board.
- Cross-compilation requires the existing local `ch32fun` toolchain; do not infer a successful target build from host-side unit tests.
- Keep `07_pps_manual_voltage_reconnect_v3` for rollback.
