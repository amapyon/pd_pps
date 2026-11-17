[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = 'C:\pj\pd_pps'
$builder = Join-Path $root 'src\pd_pps\dev-workspace\pd-pps.ps1'
if (-not (Test-Path -LiteralPath $builder -PathType Leaf)) { throw "Builder missing: $builder" }
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $builder -Action Build -Project 'firmware/experiments/43b7_p2_session_refactor'
if ($LASTEXITCODE -ne 0) { throw "M43B-7 Build failed: $LASTEXITCODE" }
$bin = Join-Path $PSScriptRoot 'm43b7_p2_session_refactor.bin'
$elf = Join-Path $PSScriptRoot 'm43b7_p2_session_refactor.elf'
foreach ($file in @($bin,$elf)) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing build artifact: $file" }
    if ((Get-Item -LiteralPath $file).Length -le 0) { throw "Empty build artifact: $file" }
}
Write-Host 'M43B-7 Build PASS. Flash is performed only by Run-Firmware.ps1 after confirmation.'
