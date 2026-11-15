[CmdletBinding()]
param([string]$Root='C:\pj\pd_pps')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
try {
    $project = Join-Path $Root 'src\pd_pps\firmware\experiments\43b6_p0p1_refactor'
    $elf = Join-Path $project 'm43b6_p0p1_refactor.elf'
    $decoder = Join-Path $project 'Decode-M43B5-SRAM.py'
    $nm = Join-Path $Root 'tools\xpack-riscv-none-elf-gcc-14.2.0-3\bin\riscv-none-elf-nm.exe'
    $link = Join-Path $Root 'src\pd_pps\ch32fun\minichlink\minichlink.exe'
    foreach ($p in @($elf,$decoder,$nm,$link)) {
        if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { throw "Missing file: $p" }
    }
    $nmLines = @(& $nm -n $elf)
    if ($LASTEXITCODE -ne 0) { throw "nm failed: $LASTEXITCODE" }
    $matchesFound = @($nmLines | Where-Object { $_ -match '^([0-9a-fA-F]+)\s+[BbDd]\s+m43b3_sram$' })
    if ($matchesFound.Count -ne 1) { throw "Expected exactly one m43b3_sram global symbol; found $($matchesFound.Count)" }
    $symbolLine=[string]$matchesFound[0]
    if ($symbolLine -notmatch '^([0-9a-fA-F]+)\s+[BbDd]\s+m43b3_sram$') { throw 'Bad symbol line' }
    $addr = [Convert]::ToInt64($Matches[1],16)
    if ($addr -lt 0x20000000 -or ($addr + 1088) -gt 0x20005000 -or ($addr % 4) -ne 0) {
        throw ('SRAM symbol outside expected range: 0x{0:x8}' -f $addr)
    }
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss_fff'
    $dir = Join-Path $Root ('diagnostics\m43b6_p0p1_refactor\capture_' + $stamp)
    if (Test-Path -LiteralPath $dir) { throw "Output directory exists: $dir" }
    Write-Host 'M43B-6 P0P1 SRAM read (halts CPU; do not reset or remove power)' -ForegroundColor Yellow
    Write-Host ('Global m43b3_sram address: 0x{0:x8}' -f $addr)
    Write-Host "Output: $dir"
    $confirm=Read-Host 'Enter DUMP after PD cable is disconnected and minichlink -T is closed'
    if ($confirm -cne 'DUMP') { Write-Host 'CANCELED. No debug read performed.'; return }
    New-Item -ItemType Directory -Path $dir -ErrorAction Stop | Out-Null
    $raw = Join-Path $dir 'sram_20k.bin'
    $stdout=Join-Path $dir 'minichlink_read_stdout.log'
    $stderr=Join-Path $dir 'minichlink_read_stderr.log'
    $proc=Start-Process -FilePath $link -ArgumentList @('-r',('"'+$raw+'"'),'ram','20480') `
        -WorkingDirectory $project -RedirectStandardOutput $stdout -RedirectStandardError $stderr `
        -NoNewWindow -Wait -PassThru
    if ($proc.ExitCode -ne 0) { throw "SRAM read failed: ExitCode=$($proc.ExitCode)" }
    if (-not (Test-Path -LiteralPath $raw -PathType Leaf)) { throw "No SRAM dump: $raw" }
    if ((Get-Item -LiteralPath $raw).Length -ne 20480) { throw 'Unexpected SRAM size' }
    $out = Join-Path $dir 'analysis'
    & python $decoder --ram $raw --address ('0x{0:x8}' -f $addr) --out $out
    if ($LASTEXITCODE -ne 0) { throw "SRAM decoder failed: $LASTEXITCODE" }
    foreach ($item in @((Join-Path $out 'events.csv'),(Join-Path $out 'summary.json'))) {
        if (-not (Test-Path -LiteralPath $item -PathType Leaf)) { throw "Missing decode output: $item" }
    }
    Write-Host 'SRAM CAPTURE AND DECODE PASSED' -ForegroundColor Green
    Write-Host "Raw: $raw"
    Write-Host "SHA256: $((Get-FileHash -LiteralPath $raw -Algorithm SHA256).Hash)"
    Write-Host "Events: $(Join-Path $out 'events.csv')"
    Write-Host "Summary: $(Join-Path $out 'summary.json')"
    Write-Host 'The MCU may remain halted; no flash, reset, or Git command was issued.'
} catch {
    Write-Host ('STOPPED: ' + $_.Exception.Message) -ForegroundColor Red
    throw
}
