[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
python -m unittest discover -s 'firmware/experiments/39_pd_pps_diag_cdc/tests' -p 'test_*.py' -v
if ($LASTEXITCODE -ne 0) { throw 'M39 Host tests failed' }
Write-Host 'M39 Host tests PASS.'
