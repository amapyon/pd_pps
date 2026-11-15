# Experiment 27 — M5 read-only PD17 → USB CDC pre-integration

**Status (2026-11-13): host-side C integration complete; MCU firmware integration NOT built/flashed or verified.**

This independent experiment implements the real C **application-layer boundary**:

`PD17 pe_t + pd_sensor_reading_t` → M5-2 C snapshot → M5-1-compatible protocol-v7 replies → foreground-only bounded USB send/receive adapter. It also includes an asynchronous **non-blocking design** for experiment-24's EP3 timeout/re-enumeration path.

It does **not** link PD17 and USBFS together in one CH32X035 firmware yet. It does not initiate PD Power Delivery negotiation, alter PPS output, or access USB hardware. Do not mistake successful tests for live concurrent USB PD + CDC operation.

## Dependencies (read-only, never modified)

- `../17_pd_spr_sink_rebuild/src/pd_policy.h` and `pd_sensors.h` — exact PD17 structures.
- `../24_usb_cdc_auto_recover/` — proven recovery strategy, USBFS transport and 2500-ms timeout; does **not** get modified.
- `../25_m5_readonly_api/` — protocol v7 draft and test fixture.
- `../26_m5_pd_snapshot_bridge/` — M5-2 reference and snapshot behavior.

`src/m5_pd_snapshot.[ch]` is an unchanged, local copy of M5-2's adapter for the isolated experiment. Shared ch32fun, PD17, experiments 24–26, VS Code configuration, and the repository roadmap remain untouched.

## C components

| Source | Purpose |
| --- | --- |
| `m5_pd_snapshot.[ch]` | Copy one coherent, already-updated `pe_t` and sensor state; hides stale contracts, caps, and readings |
| `m5_readonly.[ch]` | Strict bounded ASCII serializer for `INFO`, `HEALTH`, `GET_CAPS`, `GET_CAP N`, `STATUS`, `MEASURE`; explicitly refuses `SET_` and `OUTPUT_` |
| `m5_session.[ch]` | Foreground RX frame parser, one-response queue, clear-on-reset and backpressure; discard unsent response on PD contract/epoch change |
| `m5_usb_io.[ch]` | Callback-based nonblocking adapter, one RX byte or one TX submission per poll; USB ISR ring and USBFS callbacks not included |
| `m5_bridge_runtime.[ch]` | Call M5-2 snapshot from main loop; no PE mutation |
| `m5_recovery_nb.[ch]` | **Proposed replacement** for experiment-24's blocking `Delay_Ms(150)` in timeout recovery; detect EP3 2500ms deadline, hold USB detach asynchronously for 150ms, ask main to reinitialize |

Protocol version 7 is a **draft**, not a commitment to a production wire ABI. Each response is at most 63 bytes including LF. `GET_CAPS`+`GET_CAP N` need a host-side epoch check after the sequence: a PD Source may change its capabilities between commands.

`MEASURE` reports signed INA226 microamps truncated toward zero into whole milliamps, matching the **existing M5-2 Python reference**. `STATUS ma` remains the *requested* current, never measured current. A sensor or contract that is not fresh is not presented as a current valid reading.

## Windows 11 host tests

From the repository root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File '.\firmware\experiments\27_m5_pd_usb_readonly_integration\Test-M5-Integration-Host.ps1'
```

- Python 3 always checks **9 reference behaviors** if the prior experiment dependencies are installed.
- Native `gcc`/`clang` executes **50 C integration test cases plus 12 existing C snapshot regression cases**.
- If there is no native GCC/Clang, the available xPack `riscv-none-elf-gcc` runs **syntax checks only**, and the script explicitly flags **C runtime tests not run on this PC**. To execute them, use a native C compiler or run `tests/run_host_test.sh` in an environment with GCC/Clang.

On Linux / Ubuntu:

```bash
cd firmware/experiments/27_m5_pd_usb_readonly_integration
bash tests/run_host_test.sh
```

The creation environment passed **50/50 C integration tests and 12/12 unchanged M5-2 C tests**, with GCC AddressSanitizer/UndefinedBehaviorSanitizer enabled. That is *not* Windows MCU testing.

## Key safety and integration limits

1. **NO MCU firmware, Flash or actual combined interrupt integration yet.** The next step requires designing a new main loop and verifying USBFS/USBPD IRQ priorities, RAM/Flash capacity and time budgets. Do not overwrite PD17 or experiment 24 in place.
2. Experiment 24's `M8F_DoReenumeration()` calls `Delay_Ms(150)` while USB is disconnected. Running this unchanged alongside live PPS could stall PD time-sensitive service. `m5_recovery_nb` is a host-tested *candidate* to remove the blocking delay; hardware attach/detach and recovery notifications have not yet been wired.
3. USB bus reset/soft reconnect invalidates any pending ASCII request and reply, and requires discarding old IRQ RX ring bytes. After reconnect a fresh PD snapshot must be published before replying to PD status/caps.
4. A queued unsent STATUS/CAPS reply is canceled if PD state, epoch or contract changes. **A USB packet already submitted cannot be recalled**; the client must re-check status/epoch and treat a disconnect as breaking a transaction.
5. PC USB disconnection must **not** itself execute any new PPS Request or change PD contract. Output-changing commands are deliberately read-only rejected.
6. User accepts testing with both J1 and the added PC USB-C attached on the current CoreBoard, but D+/D− are electrically shared. This permission is NOT proof of electrical isolation, correct power routing, or safety of arbitrary PD sources. Before a combined-port trial, record the exact source/cable, VBUS/grounds and shared USB D-net conditions. Product-level electrical isolation and protection remain a separate, unfinished M5-3 gate.
7. Raw PDO fixture values come from `17_pd_spr_sink_rebuild/tests/test_policy.c`, **not live charger capture**.

## Next acceptance gates

- Host C implementation acceptance (current experiment): pass C execution tests on target development environment, not syntax only.
- Actual firmware integration: create a *separate* CH32X035 experiment; combine PD17 event servicing, new C snapshot/serializer, and nonblocking USBFS; measure Flash/RAM, max PD service intervals and USB endpoint behavior.
- Bench: validate PC CDC + PD/PPS coexistence under recorded wiring conditions, zero/small load, confirm reconnect leaves PD contract unchanged and invalidates stale values. Do not infer product compliance from functional PASS.
