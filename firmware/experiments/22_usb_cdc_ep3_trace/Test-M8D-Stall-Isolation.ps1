#requires -Version 5.1
# M8D independent USB EP3 diagnostic stress test. No Git, firmware, PD or output changes.
[CmdletBinding()]
param(
    [ValidatePattern('^COM[0-9]+$')]
    [string]$Port = 'COM9',
    [ValidateSet('PingOnly','MeasureOnly')]
    [string]$Mode = 'PingOnly',
    [ValidateRange(1,200)]
    [int]$Count = 30,
    [ValidateRange(20,5000)]
    [int]$IntervalMs = 250,
    [ValidateRange(500,20000)]
    [int]$TimeoutMs = 3000
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$outputDir = 'C:\pj\pd_pps\diagnostics\m8d_ep3trace'
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$base = 'M8D_Stall_{0}_{1}' -f $Mode, $stamp
$csvPath = Join-Path $outputDir ($base + '.csv')
$txtPath = Join-Path $outputDir ($base + '.txt')
$serial = $null
$rows = New-Object System.Collections.ArrayList
$failure = $null
$info = ''
$statusBefore = ''
$statusAfter = ''
$sw = [System.Diagnostics.Stopwatch]::New()
try {
    $serial = [System.IO.Ports.SerialPort]::new($Port, 115200,
        [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
    $serial.Handshake = [System.IO.Ports.Handshake]::None
    $serial.ReadTimeout = $TimeoutMs
    $serial.WriteTimeout = $TimeoutMs
    $serial.NewLine = "`n"
    $serial.DtrEnable = $true
    $serial.RtsEnable = $true
    $serial.Open()
    Start-Sleep -Milliseconds 300
    $serial.DiscardInBuffer()

    $serial.Write("INFO`n")
    $info = $serial.ReadLine().TrimEnd([char]13)
    if ($info -ne 'INFO board=CH32X035F8U6 firmware=m8d_usb_cdc_ep3_trace protocol=4') {
        throw "Unexpected INFO: [$info]"
    }
    $serial.Write("STATUS`n")
    $statusBefore = $serial.ReadLine().TrimEnd([char]13)
    if ($statusBefore -notmatch '^STATUS rx_packets=\d+ rx_bytes=\d+ tx_packets=\d+ tx_bytes=\d+ rx_overflows=\d+ errors=\d+$') {
        throw "Unexpected STATUS before: [$statusBefore]"
    }
    Write-Host "INFO: $info"
    Write-Host "MODE: $Mode; PORT: $Port; COUNT: $Count; INTERVAL: $IntervalMs ms"
    $sw.Start()

    for ($i = 1; $i -le $Count; $i++) {
        $command = if ($Mode -eq 'PingOnly') { 'PING' } else { 'MEASURE' }
        $serial.Write($command + "`n")
        $reply = $serial.ReadLine().TrimEnd([char]13)
        if ($Mode -eq 'PingOnly') {
            if ($reply -ne 'PONG') { throw "PING mismatch at ${i}: [$reply]" }
        } else {
            if ($reply -notmatch '^MEASURE status=ok voltage_mV=\d+ current_mA=-?\d+$') {
                throw "MEASURE mismatch at ${i}: [$reply]"
            }
        }
        [void]$rows.Add([pscustomobject][ordered]@{
            TimestampLocal = (Get-Date).ToString('yyyy-MM-ddTHH:mm:ss.fffzzz')
            ElapsedMs = $sw.ElapsedMilliseconds
            Iteration = $i
            Mode = $Mode
            Command = $command
            Response = $reply
        })
        Write-Host ("{0,3}/{1}: {2}" -f $i, $Count, $reply)
        if ($i -lt $Count) { Start-Sleep -Milliseconds $IntervalMs }
    }

    $serial.Write("STATUS`n")
    $statusAfter = $serial.ReadLine().TrimEnd([char]13)
    if ($statusAfter -notmatch '^STATUS rx_packets=\d+ rx_bytes=\d+ tx_packets=\d+ tx_bytes=\d+ rx_overflows=0 errors=\d+$') {
        throw "Unexpected STATUS after: [$statusAfter]"
    }
} catch {
    $failure = $_.Exception.Message
} finally {
    $sw.Stop()
    if ($null -ne $serial) {
        if ($serial.IsOpen) { $serial.Close() }
        $serial.Dispose()
    }
}

if ($rows.Count -gt 0) {
    $rows | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8
}
$summary = @(
    "DateTimeLocal: $((Get-Date).ToString('yyyy-MM-ddTHH:mm:ss.fffzzz'))",
    "Port: $Port",
    "Mode: $Mode",
    "Requested: $Count",
    "Completed: $($rows.Count)",
    "IntervalMs: $IntervalMs",
    "TimeoutMs: $TimeoutMs",
    "INFO: $info",
    "STATUS_before: $statusBefore",
    "STATUS_after: $statusAfter",
    "CSV: $csvPath",
    "Error: $failure",
    "Outcome: $(if ($null -eq $failure) { 'PASS' } else { 'INCOMPLETE' })"
)
[System.IO.File]::WriteAllLines($txtPath, $summary,
    [System.Text.UTF8Encoding]::new($true))
Write-Host "LOG: $txtPath"
if ($rows.Count -gt 0) { Write-Host "CSV: $csvPath" }
if ($null -ne $failure) {
    throw "M8D trace test failed: $failure"
}
Write-Host "PASS: $Mode ($Count/$Count)"
