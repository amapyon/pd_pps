#requires -Version 5.1
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = (Resolve-Path -LiteralPath (Join-Path $root '..\..\..')).Path
$expected = @(
    '..\17_pd_spr_sink_rebuild\src\pd_policy.h',
    '..\17_pd_spr_sink_rebuild\src\pd_sensors.h',
    '..\25_m5_readonly_api\src\m5_api.py',
    '..\26_m5_pd_snapshot_bridge\src\m5_pd_snapshot.c',
    '..\24_usb_cdc_auto_recover\m8f_recovery.c'
)
foreach ($relative in $expected) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf)) {
        throw "Missing dependency: $relative"
    }
}
$source = @('src/m5_pd_snapshot.c','src/m5_readonly.c','src/m5_session.c',
            'src/m5_bridge_runtime.c','src/m5_usb_io.c','src/m5_recovery_nb.c')
$tmp = Join-Path $env:TEMP ('pdpps_m5_27_' + [guid]::NewGuid().ToString('N'))
New-Item -Path $tmp -ItemType Directory -Force | Out-Null
Push-Location $root
try {
    Write-Host '== M5-4 pre-integration: Python reference checks ==' -ForegroundColor Cyan
    $py = Get-Command py -ErrorAction SilentlyContinue
    if ($null -ne $py) {
        & $py.Source -3 -m unittest discover -s tests -p test_wire_reference.py -v
    } else {
        $python = Get-Command python -ErrorAction SilentlyContinue
        if ($null -eq $python) { throw 'Python 3 not available.' }
        & $python.Source -m unittest discover -s tests -p test_wire_reference.py -v
    }
    if ($LASTEXITCODE -ne 0) { throw "Python reference tests failed (exit=$LASTEXITCODE)" }
    Write-Host 'PASS: Python reference 9/9 (not a C runtime test)' -ForegroundColor Green

    $compiler = Get-Command gcc -ErrorAction SilentlyContinue
    if ($null -eq $compiler) { $compiler = Get-Command clang -ErrorAction SilentlyContinue }
    if ($null -ne $compiler) {
        Write-Host '== Native C runtime integration tests ==' -ForegroundColor Cyan
        $exe1 = Join-Path $tmp 'm5_27_integration.exe'
        $args1 = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-Isrc',
                    'tests/test_m5_integration.c') + $source + @('-o',$exe1)
        & $compiler.Source @args1
        if ($LASTEXITCODE -ne 0) { throw "Native C compilation failed: $LASTEXITCODE" }
        & $exe1
        if ($LASTEXITCODE -ne 0) { throw "C integration runtime failed: $LASTEXITCODE" }
        Write-Host 'PASS: Native C PD17/USB protocol/recovery integration 50/50' -ForegroundColor Green
        $exe2 = Join-Path $tmp 'm5_27_snapshot.exe'
        & $compiler.Source '-std=c11' '-Wall' '-Wextra' '-Werror' '-O0' '-Isrc' `
            'tests/test_existing_snapshot.c' 'src/m5_pd_snapshot.c' '-o' $exe2
        if ($LASTEXITCODE -ne 0) { throw "Native C snapshot compile failed: $LASTEXITCODE" }
        & $exe2
        if ($LASTEXITCODE -ne 0) { throw "Native C snapshot runtime failed: $LASTEXITCODE" }
        Write-Host 'PASS: Existing M5-2 C snapshot regression 12/12' -ForegroundColor Green
    } else {
        Write-Host 'INFO: Native GCC/Clang not found. C RUNTIME TESTS NOT RUN ON THIS PC.' -ForegroundColor Yellow
        $cross = Join-Path $repo '..\..\tools\xpack-riscv-none-elf-gcc-14.2.0-3\bin\riscv-none-elf-gcc.exe'
        if (Test-Path -LiteralPath $cross -PathType Leaf) {
            Write-Host '== RISC-V C syntax checks (not execution tests) ==' -ForegroundColor Cyan
            foreach ($f in ($source + @('tests/test_m5_integration.c','tests/test_existing_snapshot.c'))) {
                & $cross '-std=c11' '-Wall' '-Wextra' '-Werror' '-fsyntax-only' '-Isrc' $f
                if ($LASTEXITCODE -ne 0) { throw "Cross syntax check failed: $f" }
            }
            Write-Host 'PASS: All C sources and C tests RISC-V syntax-checked.' -ForegroundColor Green
        } else {
            Write-Host 'WARNING: Neither native C nor RISC-V cross compiler found; C remains unverified on this PC.' -ForegroundColor Yellow
        }
    }
    Write-Host 'M5 integration tests finished. This is NOT USB/PD firmware; no flash performed.'
}
finally {
    Pop-Location
    Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
}
