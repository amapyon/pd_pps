# M43B-6 P0/P1 behavior-preserving refactor

Target: WeAct CH32X035F8U6. Separate experiment folder; original M43B-5
remains unchanged. Build target is `m43b6_p0p1_refactor`.

## P0: reproducible behavior gates

- A pristine M43B-5 `pd_policy.c` and `m38_usb.c` fixture is included **only
  under tests/fixtures** (not linked into firmware).
- The original policy fixture SHA256 is pinned. A native C program compiles
  the original under renamed symbols and runs the refactored policy alongside
  it, checking every byte of the policy structure after 32 deterministic
  sequences of 10,000 random steps plus directed attach/caps/ready/refresh/
  Hard Reset/VBUS fault cases.
- A second executable checks full CDC output equivalence for VBLOG, PDTRACE,
  and existing command responses. Existing host tests remain in place.
- Hash checks pin all 38 unchanged C/H source files and the USBFS driver.
- If native GCC/Clang is missing, Python unittest marks the native cases as
  SKIP. Run-Firmware.ps1 requires explicit `SKIP_NATIVE` to proceed without a
  native compiler; do not mistake that for a local parity PASS.

## P1: minimal code duplication removal

- `src/pd_policy.c`: a common `enter_recovery_after_power_loss` routine now
  handles identical contract clearing for VBUS/sensor fault and Hard Reset;
  original event guards, `awaiting_hr_recovery` values and fault counts remain.
- `src/m38_usb.c`: `parse_bounded_index` replaces duplicate decimal parsing
  for VBLOG 0..39 and PDTRACE 0..126. Existing error strings remain.
- Other `src/` sources and `m28_fsusb.c` are identical to M43B-5 v01.
- No changes to USBFS/PD IRQ, PHY, timing constants, USB descriptors,
  CDC command strings, SRAM ABI, ADC calibration or GPIO settings.

## Hardware caution

The original USB-ON GoodCRC failure is still present until demonstrated
otherwise. No new fix is claimed. The formal cross-link and flash are
performed in the user's Windows development environment. After flashing,
run R01 USB-OFF first, then R02 USB-ON-IDLE, preserving all logs. Keep
PD charger and PC CDC detached while flashing; keep WCH-LinkE target
power stable. Do not modify GND or CC wiring during a powered trial.
