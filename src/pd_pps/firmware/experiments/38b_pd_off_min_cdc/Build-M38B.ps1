[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\dev-workspace\pd-pps.ps1' -Action Build -Project 'firmware/experiments/38b_pd_off_min_cdc'
if ($LASTEXITCODE -ne 0) { throw 'M38B Build failed' }
Write-Host 'M38B Build PASS. This script never flashes.'
