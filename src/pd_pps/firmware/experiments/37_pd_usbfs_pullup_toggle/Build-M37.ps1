[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
& powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File '.\dev-workspace\pd-pps.ps1' `
    -Action Build -Project 'firmware/experiments/37_pd_usbfs_pullup_toggle'
if ($LASTEXITCODE -ne 0) { throw 'M37 Build failed' }
Write-Host 'M37 Build PASS. This script never flashes.'
