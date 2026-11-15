[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
python -m unittest discover -s 'firmware/experiments/42_pd_vbus_ina_cdc/tests' -p 'test_*.py' -v
if ($LASTEXITCODE -ne 0) { throw 'M42 Host tests failed' }
Write-Host 'M42 Host tests PASS.'
