#requires -Version 5.1
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = (Resolve-Path -LiteralPath (Join-Path $root '..\..\..')).Path
$exePath = Join-Path $env:TEMP 'pdpps_m5_2_snapshot_host_test.exe'
Push-Location $root
try {
    if (-not (Test-Path -LiteralPath '..\25_m5_readonly_api\src\m5_api.py')) {
        throw 'Experiment 25 (M5-1) is required. Install M5-1 before running M5-2.'
    }
    if (-not (Test-Path -LiteralPath '..\17_pd_spr_sink_rebuild\src\pd_policy.h')) {
        throw 'Original PD17 policy header not found.'
    }
    Write-Host '== M5-2 Python reference and M5-1 wire format integration ==' -ForegroundColor Cyan
    $py = Get-Command py -ErrorAction SilentlyContinue
    if ($null -ne $py) {
        & py -3 -m unittest discover -s tests -p test_reference.py -v
    } else {
        $python = Get-Command python -ErrorAction SilentlyContinue
        if ($null -eq $python) { throw 'Python 3 executable not found in PATH.' }
        & python -m unittest discover -s tests -p test_reference.py -v
    }
    if ($LASTEXITCODE -ne 0) { throw "M5-2 Python tests failed, exit=$LASTEXITCODE" }
    Write-Host 'PASS: M5-2 Python reference (12/12)' -ForegroundColor Green

    $native = Get-Command gcc -ErrorAction SilentlyContinue
    if ($null -eq $native) { $native = Get-Command clang -ErrorAction SilentlyContinue }
    if ($null -ne $native) {
        Write-Host '== M5-2 native C adapter tests ==' -ForegroundColor Cyan
        & $native.Source '-std=c11' '-Wall' '-Wextra' '-Werror' '-O0' `
            'tests/test_m5_pd_snapshot.c' 'src/m5_pd_snapshot.c' '-o' $exePath
        if ($LASTEXITCODE -ne 0) { throw "Native C compile failed, exit=$LASTEXITCODE" }
        & $exePath
        if ($LASTEXITCODE -ne 0) { throw "Native C host test failed, exit=$LASTEXITCODE" }
        Write-Host 'PASS: M5-2 native C adapter (12/12)' -ForegroundColor Green
    } else {
        Write-Host 'INFO: Native C compiler (gcc/clang) not found; C runtime tests skipped.' -ForegroundColor Yellow
        $cross = Join-Path $repo '..\..\tools\xpack-riscv-none-elf-gcc-14.2.0-3\bin\riscv-none-elf-gcc.exe'
        if (Test-Path -LiteralPath $cross) {
            & $cross '-std=c11' '-Wall' '-Wextra' '-Werror' '-fsyntax-only' 'src/m5_pd_snapshot.c'
            if ($LASTEXITCODE -ne 0) { throw "RISC-V syntax check failed, exit=$LASTEXITCODE" }
            Write-Host 'PASS: C adapter RISC-V syntax check (not a runtime test)' -ForegroundColor Green
        } else {
            Write-Host 'INFO: RISC-V cross compiler also not found. C source not checked on this PC.' -ForegroundColor Yellow
        }
    }
    Write-Host 'M5-2 host test sequence finished. No MCU flash/PD/USB operations.' -ForegroundColor Green
}
finally {
    Pop-Location
    if (Test-Path -LiteralPath $exePath) {
        Remove-Item -LiteralPath $exePath -Force -ErrorAction SilentlyContinue
    }
}
