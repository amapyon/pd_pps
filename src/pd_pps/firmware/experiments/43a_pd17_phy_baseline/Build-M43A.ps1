[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\dev-workspace\pd-pps.ps1' -Action Build -Project 'firmware/experiments/43a_pd17_phy_baseline'
if ($LASTEXITCODE -ne 0) { throw 'M43A Build failed' }
Write-Host 'M43A Build PASS. This script never flashes.'
