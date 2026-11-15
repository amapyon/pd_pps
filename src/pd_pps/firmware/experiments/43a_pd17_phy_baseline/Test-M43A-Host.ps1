[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
Set-Location $repo
python -m unittest discover -s 'firmware/experiments/43a_pd17_phy_baseline/tests' -p 'test_*.py' -v
if ($LASTEXITCODE -ne 0) { throw 'M43A Host tests failed' }
Write-Host 'M43A Host tests PASS.'
