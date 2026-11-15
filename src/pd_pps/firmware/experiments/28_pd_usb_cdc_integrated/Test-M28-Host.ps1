[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$e = $PSScriptRoot
Set-Location $e
Write-Host '== M28 static source/assembly checks =='
& python -m unittest discover -s tests -p test_source_structure.py -v
if ($LASTEXITCODE -ne 0) { throw "M28 static checks failed" }
$native = Get-Command gcc -ErrorAction SilentlyContinue
if (-not $native) { $native = Get-Command clang -ErrorAction SilentlyContinue }
$files = @(
  'src/m5_bridge_runtime.c','src/m5_pd_snapshot.c','src/m5_readonly.c',
  'src/m5_session.c','src/m5_recovery_nb.c','src/m5_usb_io.c',
  'src/m28_transport.c','src/m28_runtime.c','tests/test_m28_runtime.c'
)
if ($native) {
    Write-Host '== M28 native C host runtime assertions =='
    $testexe = Join-Path ([System.IO.Path]::GetTempPath()) ('M28_test_' + [guid]::NewGuid().ToString('N') + '.exe')
    try {
        & $native.Source -std=c11 -Wall -Wextra -Werror -pedantic -Isrc @files -o $testexe
        if ($LASTEXITCODE -ne 0) { throw 'M28 C host compilation failed' }
        & $testexe
        if ($LASTEXITCODE -ne 0) { throw 'M28 C host execution failed' }
    } finally { Remove-Item $testexe -Force -ErrorAction SilentlyContinue }
} else {
    Write-Host 'INFO: No native GCC/Clang. C HOST RUNTIME TESTS NOT EXECUTED.'
}
$repo = (Resolve-Path (Join-Path $e '..\..\..')).Path
$tools = Join-Path $repo '..\..\tools\xpack-riscv-none-elf-gcc-14.2.0-3\bin\riscv-none-elf-gcc.exe'
if (Test-Path -LiteralPath $tools) {
    Write-Host '== M28 portable C RISC-V syntax check (not a target build) =='
    & $tools -std=c11 -Wall -Wextra -Werror -fsyntax-only -Isrc @files
    if ($LASTEXITCODE -ne 0) { throw 'M28 C RISC-V syntax check failed' }
} else {
    Write-Host "INFO: RISC-V GCC not located at $tools. Cross syntax check skipped."
}
Write-Host 'M28 source/host checks complete. They DO NOT prove USBPD+USBFS coexistence on hardware.'
