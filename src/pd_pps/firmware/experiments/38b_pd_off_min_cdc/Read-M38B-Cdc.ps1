[CmdletBinding()]
param([string]$Port = 'COM11')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$sp = New-Object System.IO.Ports.SerialPort($Port,115200,'None',8,'One')
$sp.NewLine = "`n"
$sp.ReadTimeout = 2500
$sp.WriteTimeout = 2500
try {
    $sp.Open()
    Start-Sleep -Milliseconds 350
    $sp.DiscardInBuffer()
    foreach ($cmd in @('PING','STAT','COUNT')) {
        $sp.Write($cmd + "`n")
        $answer = $sp.ReadLine().Trim()
        Write-Host ($cmd + ' : ' + $answer)
        if ($cmd -eq 'PING' -and $answer -ne 'PONG M38B') {
            throw ('M38B did not reply to PING on ' + $Port)
        }
        if ($cmd -eq 'STAT' -and !$answer.StartsWith('STAT pd=off ')) {
            throw ('Unexpected STAT reply: ' + $answer)
        }
        if ($cmd -eq 'COUNT' -and !$answer.StartsWith('COUNT set=')) {
            throw ('Unexpected COUNT reply: ' + $answer)
        }
    }
    Write-Host 'M38B USBPD-off CDC read-only commands PASS.'
} finally {
    if ($sp.IsOpen) { $sp.Close() }
    $sp.Dispose()
}
