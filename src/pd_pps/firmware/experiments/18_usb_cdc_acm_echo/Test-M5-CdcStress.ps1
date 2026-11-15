# Windows PowerShell 5.1 and PowerShell 7 compatible CDC-ACM sequential echo test.
# Does not change the MCU firmware, the repository, or Windows device settings.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^COM[0-9]+$')]
    [string] $Port,

    [ValidateRange(1, 1000)]
    [int] $Rounds = 10,

    [ValidateRange(1, 100)]
    [int] $ReopenCycles = 3,

    [ValidateRange(0, 2000)]
    [int] $PauseMs = 25
)

$ErrorActionPreference = 'Stop'
$lengths = @(1, 7, 31, 63, 64, 65, 127, 128)
$totalCases = 0
$totalBytes = 0

function Invoke-VerifyEcho {
    param(
        [System.IO.Ports.SerialPort] $Serial,
        [int] $Length,
        [int] $Seed
    )

    $sent = [byte[]]::new($Length)
    for ($i = 0; $i -lt $Length; $i++) {
        $sent[$i] = [byte](($Seed + 29 * $i) -band 255)
    }

    $Serial.Write($sent, 0, $Length)
    $recv = [byte[]]::new($Length)
    $count = 0
    $deadline = [System.Diagnostics.Stopwatch]::StartNew()
    while ($count -lt $Length -and $deadline.ElapsedMilliseconds -lt 5000) {
        try {
            $count += $Serial.Read($recv, $count, $Length - $count)
        }
        catch [System.TimeoutException] {
            continue
        }
    }
    if ($count -ne $Length) {
        throw "Echo timeout: length=$Length received=$count expected=$Length"
    }
    for ($i = 0; $i -lt $Length; $i++) {
        if ($recv[$i] -ne $sent[$i]) {
            throw "Echo mismatch: length=$Length byte=$i expected=$($sent[$i]) received=$($recv[$i])"
        }
    }
}

Write-Host "M5 CDC echo test: $Port, $Rounds rounds, $ReopenCycles reopen cycles"
try {
    for ($cycle = 1; $cycle -le $ReopenCycles; $cycle++) {
        $serial = $null
        try {
            $serial = [System.IO.Ports.SerialPort]::new($Port, 115200, 'None', 8, 'One')
            $serial.Handshake = [System.IO.Ports.Handshake]::None
            $serial.ReadTimeout = 500
            $serial.WriteTimeout = 3000
            $serial.DtrEnable = $true
            $serial.RtsEnable = $true
            $serial.Open()
            Start-Sleep -Milliseconds 300
            $serial.DiscardInBuffer()
            for ($round = 1; $round -le $Rounds; $round++) {
                foreach ($length in $lengths) {
                    try {
                        Invoke-VerifyEcho -Serial $serial -Length $length -Seed ($cycle * 1000 + $round * 17 + $length)
                    }
                    catch {
                        throw "cycle=$cycle round=$round length=$length : $($_.Exception.Message)"
                    }
                    $totalCases++
                    $totalBytes += $length
                    if ($PauseMs -gt 0) { Start-Sleep -Milliseconds $PauseMs }
                }
            }
            Write-Host "PASS: cycle $cycle/$ReopenCycles"
        }
        finally {
            if ($null -ne $serial) {
                if ($serial.IsOpen) { $serial.Close() }
                $serial.Dispose()
            }
        }
        if ($cycle -lt $ReopenCycles) { Start-Sleep -Milliseconds 300 }
    }
    Write-Host "PASS: $totalCases packets, $totalBytes bytes echoed, $ReopenCycles COM open/close cycles via $Port" -ForegroundColor Green
}
catch {
    Write-Error "FAIL: $($_.Exception.Message)"
    exit 1
}
