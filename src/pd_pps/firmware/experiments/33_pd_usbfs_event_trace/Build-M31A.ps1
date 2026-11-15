[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
& powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File '.\dev-workspace\pd-pps.ps1' `
    -Action Build -Project 'firmware/experiments/31a_pd_usbfs_pwrclock'
if ($LASTEXITCODE -ne 0) { throw 'M31A Build failed' }
Write-Host 'M31A Build PASS. This script never flashes.'
