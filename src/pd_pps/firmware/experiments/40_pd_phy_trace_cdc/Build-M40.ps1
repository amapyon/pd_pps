[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\dev-workspace\pd-pps.ps1' -Action Build -Project 'firmware/experiments/40_pd_phy_trace_cdc'
if ($LASTEXITCODE -ne 0) { throw 'M40 Build failed' }
Write-Host 'M40 Build PASS. This script never flashes.'
