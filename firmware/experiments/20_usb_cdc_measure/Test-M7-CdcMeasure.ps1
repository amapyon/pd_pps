# M7 CDC measurement command test for Windows PowerShell 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^COM[0-9]+$')]
    [string]$Port,
    [ValidateRange(500,20000)]
    [int]$TimeoutMs = 3000
)
$ErrorActionPreference = 'Stop'
$serial = $null
try {
    $serial = [System.IO.Ports.SerialPort]::new($Port,115200,'None',8,'One')
    $serial.Handshake = [System.IO.Ports.Handshake]::None
    $serial.ReadTimeout = $TimeoutMs
    $serial.WriteTimeout = $TimeoutMs
    $serial.DtrEnable = $true
    $serial.RtsEnable = $true
    $serial.NewLine = "`n"
    $serial.Open()
    Start-Sleep -Milliseconds 300
    $serial.DiscardInBuffer()

    function Request([string]$message) {
        $serial.Write($message + "`n")
        return ($serial.ReadLine().TrimEnd([char]13))
    }
    $info = Request 'INFO'
    if ($info -ne 'INFO board=CH32X035F8U6 firmware=m7_usb_cdc_measure protocol=2') {
        throw "Unexpected INFO response: [$info]"
    }
    Write-Host "PASS: INFO -> $info"

    $status = Request 'STATUS'
    if ($status -notmatch '^STATUS rx_packets=\d+ rx_bytes=\d+ tx_packets=\d+ tx_bytes=\d+ rx_overflows=\d+ errors=\d+$') {
        throw "Unexpected STATUS response: [$status]"
    }
    Write-Host "PASS: STATUS -> $status"

    # M7 default device backend is deliberately NOT a sensor simulator.
    $measurement = Request 'MEASURE'
    $expected = 'MEASURE status=unavailable reason=not_configured voltage_mV=NA current_mA=NA'
    if ($measurement -ne $expected) {
        throw "Unexpected MEASURE response: [$measurement]"
    }
    Write-Host "PASS: MEASURE -> $measurement"

    $ping = Request 'PING'
    if ($ping -ne 'PONG') { throw "Unexpected PING response: [$ping]" }
    Write-Host 'PASS: PING -> PONG'

    $help = Request 'HELP'
    if ($help -ne 'HELP INFO STATUS MEASURE PING HELP') { throw "Unexpected HELP response: [$help]" }
    Write-Host 'PASS: HELP'

    $unknown = Request 'UNKNOWN'
    if ($unknown -ne 'ERR unknown_command') { throw "Unexpected UNKNOWN response: [$unknown]" }
    Write-Host 'PASS: unknown command'

    $long = Request ('X' * 40)
    if ($long -ne 'ERR line_too_long') { throw "Unexpected long command: [$long]" }
    Write-Host 'PASS: overlong command'

    for ($i=0; $i -lt 20; $i++) {
        $p = Request 'PING'
        if ($p -ne 'PONG') { throw "PING failed at iteration ${i}: [$p]" }
    }
    Write-Host "PASS: 20 PING round-trips via $Port"
    for ($i=0; $i -lt 10; $i++) {
        $m = Request 'MEASURE'
        if ($m -ne $expected) { throw "MEASURE failed at iteration ${i}: [$m]" }
    }
    Write-Host "PASS: 10 unavailable MEASURE responses (no fabricated voltage/current) via $Port"
}
finally {
    if ($null -ne $serial) {
        if ($serial.IsOpen) { $serial.Close() }
        $serial.Dispose()
    }
}
