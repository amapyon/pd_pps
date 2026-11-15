[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
& powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File '.\dev-workspace\pd-pps.ps1' `
    -Action Build -Project 'firmware/experiments/33_pd_usbfs_event_trace'
if ($LASTEXITCODE -ne 0) { throw 'M33 Build failed' }
Write-Host 'M33 Build PASS. This script never flashes.'
