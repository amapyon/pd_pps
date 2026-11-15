[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\dev-workspace\pd-pps.ps1' -Action Build -Project 'firmware/experiments/39_pd_pps_diag_cdc'
if ($LASTEXITCODE -ne 0) { throw 'M39 Build failed' }
Write-Host 'M39 Build PASS. This script never flashes.'
