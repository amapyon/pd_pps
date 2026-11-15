[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\dev-workspace\pd-pps.ps1' -Action Build -Project 'firmware/experiments/41_pd_rx_register_cdc'
if ($LASTEXITCODE -ne 0) { throw 'M41 Build failed' }
Write-Host 'M41 Build PASS. This script never flashes.'
