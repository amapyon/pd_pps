# Experiment 20: USB CDC measurement command (M7)

Target: WeActStudio CH32X035F8U6 CoreBoard, ch32fun, Windows 11.
Baseline: M6 experiment 19, verified and committed as `043194a`.

## Scope and safety

M7 introduces the `MEASURE` command and a separately compiled measurement backend.
**The CoreBoard schematic contains no external voltage/current sensor, shunt, or
measuring divider. No suitable measurement wiring has been verified for this build.**
Therefore the supplied device backend (`m7_measurement.c`) always reports
`status=unavailable` rather than inventing numeric readings. The M7 milestone
verifies the **PC/device measurement protocol**, not physical measurement.

The experiment does not drive PD CC negotiation, control VBUS, or request PPS
voltage/current. Do not assume external voltage/current can be wired directly
to the CH32X035 ADC (3.3 V domain); use an appropriate divider/protection and
current sensor after electrical limits are reviewed. PC USB-C data USB D+/D-
shares nets with existing J1: do not simultaneously attach J1 and PC USB data.
Use the independently verified board power configuration and do not connect PC
USB VBUS directly to 3.3 V.

## ASCII protocol v2

One upper-case command terminated by LF, optional preceding CR. One response
terminated by LF. Maximum command length 31 printable ASCII characters.
CDC line setting 115200 8N1 is only a Windows COM convention.

| Request | Example device response |
| --- | --- |
| `INFO` | `INFO board=CH32X035F8U6 firmware=m7_usb_cdc_measure protocol=2` |
| `STATUS` | `STATUS rx_packets=2 rx_bytes=12 tx_packets=2 tx_bytes=64 rx_overflows=0 errors=0` (counters vary) |
| `MEASURE` | `MEASURE status=unavailable reason=not_configured voltage_mV=NA current_mA=NA` |
| `PING` | `PONG` |
| `HELP` | `HELP INFO STATUS MEASURE PING HELP` |
| `UNKNOWN` | `ERR unknown_command` |

`STATUS` counters are since MCU boot. `errors` counts rejected commands, not
necessarily physical USB transport failures.

### Future measurement backend contract

`m7_measurement.h` defines `M7_ReadMeasurement(M7_Measurement*)` and statuses:

- `M7_MEASURE_OK`: actual validated sample: `MEASURE status=ok voltage_mV=<unsigned> current_mA=<signed>`
- `M7_MEASURE_NOT_CONFIGURED`: `MEASURE status=unavailable reason=not_configured voltage_mV=NA current_mA=NA`
- `M7_MEASURE_IO_ERROR`: `MEASURE status=error reason=sensor_io voltage_mV=NA current_mA=NA`
- Unknown status: `MEASURE status=error reason=backend_invalid voltage_mV=NA current_mA=NA`

The `OK` status must be returned **only after real sensor acquisition and
calibration**, never from a static constant or simulated value on target.
A later backend can use e.g. INA226 over I2C, provided the shunt value, wiring,
sensor configuration and bus-voltage range are established. The C host tests
inject fake sensor values ONLY in the host test, to verify response formatting.

## Install / Build / Flash

Use the separately supplied `Install-M7-Only.ps1`. It creates only:
`firmware/experiments/20_usb_cdc_measure`. Existing M5/M6 and unrelated files
are untouched. Build/Flash through the existing workspace script:

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
$Project = 'firmware/experiments/20_usb_cdc_measure'
.\dev-workspace\pd-pps.ps1 -Action Build -Project $Project
# Flash only after successful Build and when replacing running M6 is intended
.\dev-workspace\pd-pps.ps1 -Action Flash -Project $Project
```

After Flash, reconnect only the USB data connector, check COM device numbering:

```powershell
Get-PnpDevice -PresentOnly -Class Ports | Format-Table FriendlyName, Status, InstanceId -AutoSize
.\firmware\experiments\20_usb_cdc_measure\Test-M7-CdcMeasure.ps1 -Port COM9
```

## Offline host tests

```sh
cd firmware/experiments/20_usb_cdc_measure
gcc -std=c11 -Wall -Wextra -Werror -I. host_test/test_measure.c -o /tmp/m7_test
/tmp/m7_test
gcc -std=c11 -Wall -Wextra -Werror -I. host_test/test_backend.c m7_measurement.c -o /tmp/m7_backend
/tmp/m7_backend
```

## Hardware protocol verification (2026-11-09, JST)

Windows 11 recognized the running device on COM9. The `Test-M7-CdcMeasure.ps1`
script was run successfully once, then repeated 10 times without cable unplugging.

- All 10 repeated runs: **PASS**; each run sent 37 commands (7 basic/error
  requests + 20 more PING + 10 more MEASURE), giving **370 requests**.
- **210 PING** and **110 MEASURE** responses were verified, counting the basic
  PING and MEASURE once per run.
- **20 intentionally rejected commands** returned expected error responses;
  `STATUS errors` progressed from 2 to 20, matching the intentional rejections
  accumulated at the times the counters were sampled.
- `rx_overflows=0` in all 10 STATUS responses; no observed response mismatch
  or timeout.
- All `MEASURE` responses were `status=unavailable reason=not_configured`;
  no physical voltage or current was read.
- Details: [M7 verification record](../../../docs/M7_USB_CDC_MEASURE_VERIFICATION.md).

## Limits / release gate

- Real volts and amps: **NOT implemented** without confirmed hardware.
- Windows 11 COM9 command protocol tests: **PASS** (INFO, STATUS, MEASURE, PING, HELP, unknown and overlong command; 20 additional PING, 10 additional MEASURE).
- Continuous repeat: **10/10 PASS**, 370 requests total, 210 PING and 110 MEASURE (including basic checks), 20 deliberately invalid commands; no test timeouts or response mismatches.
- The device identified itself as `m7_usb_cdc_measure protocol=2`. The original Build/Flash logs for M7 were not included with this verification record.
- Host test numeric samples are synthetic ONLY for test; not deployed.
- USB CDC descriptors are inherited from M6 for experiments only; VID/PID
  registration, USB VBUS detection, reconnection and extended reliability
  require further work before product release.
- Do not stage or modify 16b, M5 `host_test/mock_usb.h`, M6, ROADMAP,
  ch32fun, or development tasks. Do not auto-commit/push.
