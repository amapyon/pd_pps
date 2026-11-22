# PD/PPS Explorer - Development environment standard (2026-11-22)

This is the single source of tool *requirements*, not a proof that a given PC has the tools.
The root README and DEVELOPMENT_RULES.md take precedence on project safety.

- Root: `C:\pj\pd_pps` (single Git root).
- Runtime: Windows 11; Windows PowerShell 5.1 (not PowerShell 7).
- Project compiler: xPack RISC-V GCC `riscv-none-elf-gcc.exe`, NOT native `gcc.exe`.
- Build: xPack GNU Make; use existing `src/pd_pps/dev-workspace/pd-pps.ps1` as the build helper.
- Python: local Python 3; native C compiler is optional and MUST be reported `OPTIONAL_MISSING` if absent.
- COM port numbers and Git HEAD/branch are observed at runtime, never fixed prerequisites.

## Entry points

One command per workflow. A one-shot workflow performs all safe phases automatically
(preflight, apply if explicitly requested, verification, diagnostics, log) and stops on failure.
Never require the user to run successive small command blocks for a normal workflow.
Avoid interactive prompts for safe work. Unavoidable confirmations (Flash, rewiring,
 destructive Git operations) remain separate safety gates inside the one runner.

- After initial installation: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\pj\pd_pps\scripts\project\Run-DevChecks.ps1"`
- Tool/environment probe: `scripts/project/Test-DevEnvironment.ps1` with `-Profile Inspect|Build|HostC|Hardware`.
- Resolve/execute tools: `scripts/project/Invoke-DevTool.ps1` using the config, not guessed PATH.
- Static PowerShell check: `scripts/project/Test-PowerShellSafety.ps1`.

## Quality gate for future packages

1. Check project root and live tool availability; do not assume a native compiler.
2. Parse `.ps1` using PowerShell AST and reject functions named `Git`, `Make`, `Python`, etc.
   Reject assignment to read-only automatic variables such as `$Host` and `$PID`.
3. Test install dry-run, apply, repeat application, missing required tools, and optional tools.
4. Run on Windows PowerShell 5.1 or explicitly mark `NOT_TESTED` if unavailable.
5. Run tests only in their declared environment; do not report SKIP as PASS.
6. One command at the point of use. Use `-Apply` to authorize safe file placement,
   and explicit separate `FLASH` consent for MCU writes.
7. Never automatically install a compiler, change PATH globally, alter drivers,
   commit/push, or write MCU Flash.

## Evidence for a new chat

Provide `config/dev_environment.json`, current `README.md`, `DEVELOPMENT_RULES.md`,
and the latest small `diagnostics/environment/runs/*/environment_*.json` report when needed.
The assistant cannot access the user's local `C:\pj\pd_pps` automatically.
