[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\dev-workspace\pd-pps.ps1' -Action Build -Project 'firmware/experiments/42_pd_vbus_ina_cdc'
if ($LASTEXITCODE -ne 0) { throw 'M42 Build failed' }
Write-Host 'M42 Build PASS. This script never flashes.'
