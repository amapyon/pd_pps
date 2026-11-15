# M43B-7 P1 on-device CDC parser acceptance check.
# Read-only commands; never flashes, resets, changes PD policy or dumps SRAM.
# Windows PowerShell 5.1 / CH32X035 M43B-7 v01 / COM16.
[CmdletBinding()]
param(
    [string]$Root = 'C:\pj\pd_pps',
    [ValidatePattern('^COM[0-9]+$')][string]$Com = 'COM16'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$port = $null
$writer = $null
$stream = $null
$passed = 0
$failed = 0
try {
    $answer = Read-Host 'Verify PD charger is physically DISCONNECTED, minichlink terminal is CLOSED, and MCU is running. Type DETACHED'
    if ($answer -cne 'DETACHED') { throw 'Preconditions not confirmed. No serial test performed.' }
    if ($Com -notin @([System.IO.Ports.SerialPort]::GetPortNames())) {
        throw "Serial port missing: $Com. Do not reconnect the PD charger."
    }
    $logDir = Join-Path $Root 'diagnostics\m43b7_p2_session_refactor\logs'
    New-Item -ItemType Directory -Path $logDir -Force | Out-Null
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss_fff'
    $log = Join-Path $logDir ('M43B7_R03_CDC_INDEX_' + $stamp + '.log')
    $stream = [System.IO.File]::Open($log, [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write, [System.IO.FileShare]::Read)
    $writer = [System.IO.StreamWriter]::new($stream, [System.Text.UTF8Encoding]::new($false))
    $writer.AutoFlush = $true
    $writer.WriteLine('TEST M43B7_R03_CDC_INDEX mode=PD_DETACHED com=' + $Com)
    $writer.WriteLine('START ' + (Get-Date -Format o))

    $port = [System.IO.Ports.SerialPort]::new(
        $Com, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
    $port.NewLine = "`n"
    $port.ReadTimeout = 2000
    $port.WriteTimeout = 2000
    $port.DtrEnable = $true
    $port.Open()
    Start-Sleep -Milliseconds 350
    $port.DiscardInBuffer()

    # Extracted from m38_usb.c in the M43B-7 P0/P1 release:
    # VBLOG valid indices 0..39; PDTRACE valid indices 0..126.
    $cases = @(
        @{ Command='PING';          Pattern='^PONG M43A$' },
        @{ Command='STAT';          Pattern='^STAT a=0 pe=0 pps=\d+ mv=\d+ ma=\d+ cfg=1$' },
        @{ Command='COUNT';         Pattern='^COUNT set=\d+ in=\d+ out=\d+ rst=\d+ irq=\d+ ov=0$' },
        @{ Command='VBLOG 0';       Pattern='^VBLOG idx=0 count=\d+ (?:none=1|ms=\d+ raw=\d+ mv=\d+ flags=\d+)$' },
        @{ Command='VBLOG 39';      Pattern='^VBLOG idx=39 count=\d+ (?:none=1|ms=\d+ raw=\d+ mv=\d+ flags=\d+)$' },
        @{ Command='VBLOG 000';     Pattern='^VBLOG idx=0 count=\d+ (?:none=1|ms=\d+ raw=\d+ mv=\d+ flags=\d+)$' },
        @{ Command='VBLOG 0039';    Pattern='^VBLOG idx=39 count=\d+ (?:none=1|ms=\d+ raw=\d+ mv=\d+ flags=\d+)$' },
        @{ Command='VBLOG 40';      Pattern='^ERR VBLOG index must be 0\.\.39$' },
        @{ Command='VBLOG 999999';  Pattern='^ERR VBLOG index must be 0\.\.39$' },
        @{ Command='VBLOG -1';      Pattern='^ERR VBLOG index must be 0\.\.39$' },
        @{ Command='VBLOG +1';      Pattern='^ERR VBLOG index must be 0\.\.39$' },
        @{ Command='VBLOG 1a';      Pattern='^ERR VBLOG index must be 0\.\.39$' },
        @{ Command='VBLOG ';        Pattern='^ERR VBLOG index must be 0\.\.39$' },
        @{ Command='PDTRACE 0';     Pattern='^PDTRACE idx=0 count=\d+ (?:none=1|tick=[0-9a-fA-F]{8} code=\d+ detail=\d+ aux=[0-9a-fA-F]{4} value=[0-9a-fA-F]{8})$' },
        @{ Command='PDTRACE 126';   Pattern='^PDTRACE idx=126 count=\d+ (?:none=1|tick=[0-9a-fA-F]{8} code=\d+ detail=\d+ aux=[0-9a-fA-F]{4} value=[0-9a-fA-F]{8})$' },
        @{ Command='PDTRACE 000';   Pattern='^PDTRACE idx=0 count=\d+ (?:none=1|tick=[0-9a-fA-F]{8} code=\d+ detail=\d+ aux=[0-9a-fA-F]{4} value=[0-9a-fA-F]{8})$' },
        @{ Command='PDTRACE 0126';  Pattern='^PDTRACE idx=126 count=\d+ (?:none=1|tick=[0-9a-fA-F]{8} code=\d+ detail=\d+ aux=[0-9a-fA-F]{4} value=[0-9a-fA-F]{8})$' },
        @{ Command='PDTRACE 127';   Pattern='^ERR PDTRACE index must be 0\.\.126$' },
        @{ Command='PDTRACE 999999';Pattern='^ERR PDTRACE index must be 0\.\.126$' },
        @{ Command='PDTRACE -1';    Pattern='^ERR PDTRACE index must be 0\.\.126$' },
        @{ Command='PDTRACE +1';    Pattern='^ERR PDTRACE index must be 0\.\.126$' },
        @{ Command='PDTRACE 1a';    Pattern='^ERR PDTRACE index must be 0\.\.126$' },
        @{ Command='PDTRACE ';      Pattern='^ERR PDTRACE index must be 0\.\.126$' },
        @{ Command='PING';          Pattern='^PONG M43A$' }
    )
    foreach ($test in $cases) {
        $command = [string]$test.Command
        $port.WriteLine($command)
        $reply = $port.ReadLine().Trim()
        if ($reply -match [string]$test.Pattern) {
            $passed++
            $result = 'PASS'
        } else {
            $failed++
            $result = 'FAIL'
        }
        $line = ('{0} CMD={1} REPLY={2}' -f $result, ($command.Replace(' ', '<SP>')), $reply)
        $writer.WriteLine($line)
        Write-Host $line
    }
    $writer.WriteLine(('SUMMARY pass={0} fail={1} total={2}' -f $passed, $failed, $cases.Count))
    $writer.WriteLine('END ' + (Get-Date -Format o))
    Write-Host ('Log: ' + $log)
    if ($failed -ne 0) { throw ('CDC parser tests FAILED: ' + $failed) }
    Write-Host ('R03 CDC INDEX CHECK PASSED: ' + $passed + '/' + $cases.Count) -ForegroundColor Green
}
catch {
    if ($null -ne $writer) {
        $writer.WriteLine('ERROR ' + (Get-Date -Format o) + ' ' + $_.Exception.Message)
    }
    Write-Host ('STOPPED: ' + $_.Exception.Message) -ForegroundColor Red
    throw
}
finally {
    if ($null -ne $port) {
        if ($port.IsOpen) { $port.Close() }
        $port.Dispose()
    }
    if ($null -ne $writer) { $writer.Dispose() }
    elseif ($null -ne $stream) { $stream.Dispose() }
}
