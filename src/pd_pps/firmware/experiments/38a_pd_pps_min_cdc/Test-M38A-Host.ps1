[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
Set-Location $PSScriptRoot
& python -X utf8 -m unittest discover -s tests -p 'test_*.py' -v
if ($LASTEXITCODE -ne 0) { throw 'M38A host/static tests failed' }
Write-Host 'M38A Host Test PASS. Native C test is skipped if gcc/clang is absent.'
