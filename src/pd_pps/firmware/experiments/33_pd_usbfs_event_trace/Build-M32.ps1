[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
& powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File '.\dev-workspace\pd-pps.ps1' `
    -Action Build -Project 'firmware/experiments/32_pd_usbfs_pd_interference'
if ($LASTEXITCODE -ne 0) { throw 'M32 Build failed' }
Write-Host 'M32 Build PASS. This script never flashes.'
