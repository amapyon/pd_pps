#requires -version 5.1
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $root
try {
    $python = (Get-Command py -ErrorAction SilentlyContinue)
    if ($null -ne $python) { & py -3 -m unittest discover -s tests -v; $result = $LASTEXITCODE }
    else {
        $python = (Get-Command python -ErrorAction SilentlyContinue)
        if ($null -eq $python) { throw 'Python 3 was not found in PATH. Install/use Python 3.' }
        & python -m unittest discover -s tests -v
        $result = $LASTEXITCODE
    }
    if ($result -ne 0) { throw "M5-1 host test failed (exit $result)" }
    Write-Host 'PASS: M5-1 host-only API tests' -ForegroundColor Green
}
finally { Pop-Location }
