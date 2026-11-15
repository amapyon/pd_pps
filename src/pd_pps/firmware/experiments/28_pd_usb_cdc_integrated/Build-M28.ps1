[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
& powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File '.\dev-workspace\pd-pps.ps1' `
    -Action Build -Project 'firmware/experiments/28_pd_usb_cdc_integrated'
if ($LASTEXITCODE -ne 0) { throw 'M28 Build failed' }
Write-Host 'M28 Build PASS. Do NOT flash until power/wiring and USB topology safety checks are complete.'
