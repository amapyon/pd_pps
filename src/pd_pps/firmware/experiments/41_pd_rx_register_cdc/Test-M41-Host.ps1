[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
python -m unittest discover -s 'firmware/experiments/41_pd_rx_register_cdc/tests' -p 'test_*.py' -v
if ($LASTEXITCODE -ne 0) { throw 'M41 Host tests failed' }
Write-Host 'M41 Host tests PASS.'
