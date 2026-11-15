# Experiment 26 — M5-2 PD17 read-only snapshot bridge (HOST ONLY)

**State: host implementation and verification, NOT firmware integration or hardware proof.**

This experiment builds a strictly read-only **PD17 `pe_t` + `pd_sensor_reading_t` → snapshot** boundary. The C adapter includes the actual headers from the preserved experiment 17; it does not copy, modify, link, or call the PD17 Policy Engine implementation. The Python reference maps equivalent fixture fields into the host-only v7 protocol already developed in experiment 25 (M5-1).

## Requirements

- Git repository `C:\pj\pd_pps\src\pd_pps` with original `17_pd_spr_sink_rebuild` unchanged.
- Experiment `25_m5_readonly_api` installed (M5-1 18/18 host tests PASS).
- Python 3.11+ recommended. Windows PowerShell 5.1.
- For running **native C host tests** on Windows, native GCC or Clang in PATH. Without these, the script checks the C source with the existing xPack RISC-V GCC (if present) and clearly reports that runtime C tests were skipped.

## Test (no MCU / USB / PD source)

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File '.\firmware\experiments\26_m5_pd_snapshot_bridge\Test-M5-2-Host.ps1'
```

On Linux with GCC, run both directly:

```sh
cd firmware/experiments/26_m5_pd_snapshot_bridge
python3 -m unittest discover -s tests -p 'test_reference.py' -v
gcc -std=c11 -O0 -Wall -Wextra -Werror -pedantic \
  tests/test_m5_pd_snapshot.c src/m5_pd_snapshot.c -o /tmp/m5_2_test
/tmp/m5_2_test
```

Both have 12 test cases. Native C tests compile against the **real** `firmware/experiments/17_pd_spr_sink_rebuild/src/pd_policy.h` and `pd_sensors.h` in the user's repository (not mock headers shipped in this ZIP).

## Safety and semantics

- **Main-loop ONLY**: capture a coherent `pe_t` and sensors state outside any USBFS/USBPD IRQ. Never read a partially changing Policy Engine struct.
- No `pe_event`, `pe_tick`, USB send/reopen, I²C, PWM, PD Request, PPS update or power switching calls occur in this module.
- Report Source Caps only when `caps_fresh` is true, count is in range 1..7, and the PE is not DETACHED/RECOVERY; reset raw PDOs and current contract on detach/recovery.
- Contract is asserted only for `PE_READY`, `contract_valid`, fresh compatible Source Caps, nonzero requested mV/mA and an in-range 1-based PDO index. This *intentionally* suppresses contract attribution during negotiation (it does not claim physical VBUS is necessarily off).
- `requested_ma` is a requested **source limit**. `ina_current_ua` is measured signed microamps. Python M5-1 wire format converts to **integer mA by truncating toward zero**, never substituting requested current.
- ADC validity requires both `adc_ok` and `adc_calibrated_range`. Sensor readings are not published without an explicit `sensor_fresh` flag from the caller.
- `caps_epoch` is not a globally unique reconnect ID; host MUST re-read after reconnect. No products or devices are connected for these tests.

## Verification status and next gate

- Local GNU GCC C adapter tests: 12/12 PASS against a test-local transcription of the PD17 header layout, later compile on user's actual repository required.
- Portable Python bridge + M5-1 text API tests: 12/12 PASS with original M5-1 fixture and PD17 test PDO data.
- Windows actual compiler behavior, MCU Build/Flash, real PD/USB simultaneously: **not checked**.

**Next:** M5-3 electrical safety gate for simultaneous PC and PD, or first integrate this snapshot interface into a new MCU firmware only after choosing a safe connection design. Do **not** connect CoreBoard J1 to a PD source and the added PC USB-C to a PC simultaneously: both currently share D+/D−.

Refer to `docs/M5_NEXT_PC_CONTROL_PLAN.md` and `docs/HARDWARE_WIRING.md` in the repository.
