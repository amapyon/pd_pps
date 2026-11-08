param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^COM[0-9]+$')]
    [string]$Port
)

$ErrorActionPreference = 'Stop'
$serial = $null
try {
    $serial = [System.IO.Ports.SerialPort]::new($Port, 115200, 'None', 8, 'One')
    $serial.Handshake = [System.IO.Ports.Handshake]::None
    $serial.ReadTimeout = 3000
    $serial.WriteTimeout = 3000
    $serial.DtrEnable = $true
    $serial.RtsEnable = $true
    $serial.Open()
    Start-Sleep -Milliseconds 300
    $serial.DiscardInBuffer()

    $payload = [System.Text.Encoding]::ASCII.GetBytes('PD-PPS M5 USB CDC-ACM echo 0123456789')
    $serial.Write($payload, 0, $payload.Length)

    $received = [byte[]]::new($payload.Length)
    $count = 0
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($count -lt $payload.Length -and $stopwatch.ElapsedMilliseconds -lt 5000) {
        try {
            $count += $serial.Read($received, $count, $payload.Length - $count)
        }
        catch [System.TimeoutException] {
            break
        }
    }
    if ($count -ne $payload.Length) {
        throw "FAIL: Received $count of $($payload.Length) bytes."
    }
    for ($i = 0; $i -lt $payload.Length; $i++) {
        if ($received[$i] -ne $payload[$i]) {
            throw "FAIL: Different byte at index $i."
        }
    }
    Write-Host "PASS: $count bytes echo-matched via $Port" -ForegroundColor Green
}
finally {
    if ($null -ne $serial) {
        if ($serial.IsOpen) { $serial.Close() }
        $serial.Dispose()
    }
}
