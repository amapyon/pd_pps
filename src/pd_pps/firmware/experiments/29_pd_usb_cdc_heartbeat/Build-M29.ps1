[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
& powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File '.\dev-workspace\pd-pps.ps1' `
    -Action Build -Project 'firmware/experiments/29_pd_usb_cdc_heartbeat'
if ($LASTEXITCODE -ne 0) { throw 'M29 Build failed' }
Write-Host 'M29 Build PASS. This script never flashes.'
