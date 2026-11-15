[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
& powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File '.\dev-workspace\pd-pps.ps1' `
    -Action Build -Project 'firmware/experiments/34_pd_usbfs_phase_trace'
if ($LASTEXITCODE -ne 0) { throw 'M34 Build failed' }
Write-Host 'M34 Build PASS. This script never flashes.'
