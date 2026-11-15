[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
& powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File '.\dev-workspace\pd-pps.ps1' `
    -Action Build -Project 'firmware/experiments/35_pd_usbfs_phy_isolation'
if ($LASTEXITCODE -ne 0) { throw 'M35 Build failed' }
Write-Host 'M35 Build PASS. This script never flashes.'
