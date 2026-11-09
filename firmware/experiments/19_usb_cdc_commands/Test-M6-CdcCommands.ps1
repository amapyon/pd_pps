# Windows PowerShell 5.1: request/response smoke test for M6 USB CDC commands.
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
    if ($info -ne 'INFO board=CH32X035F8U6 firmware=m6_usb_cdc_commands protocol=1') {
        throw "Unexpected INFO response: [$info]"
    }
    Write-Host "PASS: INFO -> $info"

    $status = Request 'STATUS'
    if ($status -notmatch '^STATUS rx_packets=\d+ rx_bytes=\d+ tx_packets=\d+ tx_bytes=\d+ rx_overflows=\d+ errors=\d+$') {
        throw "Unexpected STATUS response: [$status]"
    }
    Write-Host "PASS: STATUS -> $status"

    $ping = Request 'PING'
    if ($ping -ne 'PONG') { throw "Unexpected PING response: [$ping]" }
    Write-Host 'PASS: PING -> PONG'

    $help = Request 'HELP'
    if ($help -ne 'HELP INFO STATUS PING HELP') { throw "Unexpected HELP response: [$help]" }
    Write-Host 'PASS: HELP'

    $unknown = Request 'UNKNOWN'
    if ($unknown -ne 'ERR unknown_command') { throw "Unexpected unknown-command response: [$unknown]" }
    Write-Host 'PASS: unknown command -> ERR unknown_command'

    $long = Request ('X' * 40)
    if ($long -ne 'ERR line_too_long') { throw "Unexpected long-command response: [$long]" }
    Write-Host 'PASS: long command -> ERR line_too_long'

    # Exercise short-message handling repeatedly without reopening COM.
    for ($i=0; $i -lt 20; $i++) {
        $p = Request 'PING'
        if ($p -ne 'PONG') { throw "PING failed at iteration ${i}: [$p]" }
    }
    Write-Host "PASS: 20 consecutive PING round-trips via $Port" -ForegroundColor Green
}
finally {
    if ($null -ne $serial) {
        if ($serial.IsOpen) { $serial.Close() }
        $serial.Dispose()
    }
}
