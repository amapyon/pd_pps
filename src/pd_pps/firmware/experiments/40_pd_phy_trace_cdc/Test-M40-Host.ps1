[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
python -m unittest discover -s 'firmware/experiments/40_pd_phy_trace_cdc/tests' -p 'test_*.py' -v
if ($LASTEXITCODE -ne 0) { throw 'M40 Host tests failed' }
Write-Host 'M40 Host tests PASS.'
