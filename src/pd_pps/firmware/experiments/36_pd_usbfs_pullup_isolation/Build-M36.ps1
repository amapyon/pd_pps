[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
& powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File '.\dev-workspace\pd-pps.ps1' `
    -Action Build -Project 'firmware/experiments/36_pd_usbfs_pullup_isolation'
if ($LASTEXITCODE -ne 0) { throw 'M36 Build failed' }
Write-Host 'M36 Build PASS. This script never flashes.'
