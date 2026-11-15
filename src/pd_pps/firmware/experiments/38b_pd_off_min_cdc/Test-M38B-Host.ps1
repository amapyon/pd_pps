[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Set-Location $PSScriptRoot
& python -X utf8 -m unittest discover -s tests -p 'test_*.py' -v
if ($LASTEXITCODE -ne 0) { throw 'M38B host/static tests failed' }
Write-Host 'M38B Host Test PASS. Native C runtime test skips when gcc/clang is absent.'
