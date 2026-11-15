[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
& python -m unittest discover -s (Join-Path $PSScriptRoot 'tests') -p 'test_*.py' -v
if ($LASTEXITCODE -ne 0) { throw "M43B-6 P0/P1 host tests failed: $LASTEXITCODE" }
Write-Host 'M43B-6 P0/P1 host tests PASS'
