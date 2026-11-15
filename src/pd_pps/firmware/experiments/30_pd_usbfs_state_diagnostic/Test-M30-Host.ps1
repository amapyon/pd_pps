[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Set-Location $PSScriptRoot
& python -m unittest discover -s tests -p test_source_structure.py -v
if ($LASTEXITCODE -ne 0) { throw 'Inherited M28 static tests failed' }
& python -m unittest discover -s tests -p test_m29_heartbeat.py -v
if ($LASTEXITCODE -ne 0) { throw 'M29 heartbeat tests failed' }
& python -m unittest discover -s tests -p test_m30_diag.py -v
if ($LASTEXITCODE -ne 0) { throw 'M30 diagnostic tests failed' }
Write-Host 'M30 static checks PASS; hardware coexistence not validated.'
