[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\dev-workspace\pd-pps.ps1' -Action Build -Project 'firmware/experiments/38a_pd_pps_min_cdc'
if ($LASTEXITCODE -ne 0) { throw 'M38A Build failed' }
Write-Host 'M38A Build PASS. This script never flashes.'
