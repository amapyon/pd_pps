[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
& powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File '.\dev-workspace\pd-pps.ps1' `
    -Action Build -Project 'firmware/experiments/30_pd_usbfs_state_diagnostic'
if ($LASTEXITCODE -ne 0) { throw 'M30 Build failed' }
Write-Host 'M30 Build PASS. This script never flashes.'
