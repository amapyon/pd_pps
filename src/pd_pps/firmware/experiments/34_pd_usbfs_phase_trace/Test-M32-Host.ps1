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
& python -m unittest discover -s tests -p test_m31a_clock.py -v
if ($LASTEXITCODE -ne 0) { throw 'M32 PWR-clock checks failed' }
& python -m unittest discover -s tests -p test_m32_diag.py -v
if ($LASTEXITCODE -ne 0) { throw 'M32 diagnostic tests failed' }
Write-Host 'M32 static checks PASS; hardware coexistence not validated.'
