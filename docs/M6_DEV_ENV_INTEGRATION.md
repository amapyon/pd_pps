# M6 and shared development environment integration (2026-11-22)

<!-- M6_WCH_LINKE_POLICY_20261122 -->
**2026-11-22 decision superseding prior M6 USB-update experiment plans:** Product firmware programming/recovery uses **WCH-LinkE over SWD only**. PC USB-C #2 is USB CDC command/data/control only, not a firmware transfer or BOOT interface. The M6 USB ISP entry build remains a historical disabled experiment; no product integration. [Policy](M6_FIRMWARE_UPDATE_POLICY.md).
<!-- /M6_WCH_LINKE_POLICY_20261122 -->

## Scope

- Standardization v01 is installed in the local Git root and checked first.
- M6 v03 firmware experiment's 100 source/test files remain untouched.
- `scripts/project/Run-M6-Build.ps1` is the stable M6 BuildOnly entry point.
- Previous `fw_m6_usb_isp_entry_v03_20261122/Run-Firmware.ps1` is retained as an immutable historical KIT runner, **not** the default build entry point.

## Single-run behavior

The integration KIT runner does preflight, idempotent installation of the two new files, post-check,
PowerShell AST safety checks, shared environment checks, and M6 BuildOnly automatically. Optional
selective Git commit/push is explicitly requested by `-Publish` and occurs **only after** BuildOnly succeeds.
No Flash, USB reset, driver/tool installation, GPIO/PD operation, or C/D/M6 firmware source changes.

The M6 BuildOnly runner verifies that the already installed 100 files still match the v03 KIT,
that `M6_BOOT_ENTRY_ENABLED=0`, and that xPack RISC-V GCC and Make are present. If a Windows-native
compiler is absent, it reports `HOST_TESTS_NOT_TESTED` rather than failing the cross-build; this does
**not** count as host-test PASS. If it is present, the host C tests are mandatory and must PASS.
Build outputs and raw stdout/stderr are saved in `diagnostics/m6_usb_update/runs/<run-id>/`.

## Git scope (when explicitly requested)

Only these twelve paths are staged and committed on `work/m43b8b-verification-20261117`:

- `README.md`
- `DEVELOPMENT_RULES.md`
- `pd_pps_zip_rules.md`
- `config/dev_environment.json`
- `docs/DEVELOPMENT_ENVIRONMENT_STANDARD.md`
- `scripts/project/Dev-Environment.Common.ps1`
- `scripts/project/Invoke-DevTool.ps1`
- `scripts/project/Run-DevChecks.ps1`
- `scripts/project/Test-DevEnvironment.ps1`
- `scripts/project/Test-PowerShellSafety.ps1`
- `scripts/project/Run-M6-Build.ps1`
- `docs/M6_DEV_ENV_INTEGRATION.md`

The publisher checks the current branch and local/remote HEAD, empty index, exact standardization
append-only changes to the three tracked documents, and SHA-256 for the seven previously installed
standardization files and two new files before staging. It will not stage `diagnostics/`, firmware
experiments, old ZIPs, or unrelated untracked documents. No merge or force push.

**Environment documentation becoming available on GitHub does not make a user's local C:\ files
accessible to ChatGPT automatically.** Use the committed standards to guide future runs, and provide
local diagnostic reports as needed.

## Gates still open

- Windows PowerShell 5.1 runtime execution: check the user's actual terminal output.
- Windows-native C tests: NOT_TESTED until a compatible compiler is available.
- MCU Flash, USB ISP mode entry, update/recovery: NOT_TESTED and not implemented by this KIT.
