[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Port)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$sp = New-Object System.IO.Ports.SerialPort($Port,115200,'None',8,'One')
$sp.NewLine = "`n"; $sp.ReadTimeout = 4000; $sp.WriteTimeout = 4000
try {
    $sp.Open()
    Start-Sleep -Milliseconds 350
    $sp.DiscardInBuffer()
    foreach ($cmd in @('PING','STAT','COUNT','DELTA','PDSTOP','PDSTAB','PDERR','PDTINFO','VBINFO','INA')) {
        $sp.Write($cmd + "`n")
        $answer = $sp.ReadLine().Trim()
        Write-Host ($cmd + ' : ' + $answer)
        if ($cmd -eq 'PING' -and $answer -ne 'PONG M42') { throw ('Unexpected device on ' + $Port) }
        if ($cmd -ne 'PING' -and !$answer.StartsWith($cmd + ' ')) {
            throw ('Unexpected ' + $cmd + ': ' + $answer)
        }
    }
    Write-Host 'M42 CDC read-only commands PASS.'
} finally {
    if ($sp.IsOpen) { $sp.Close() }
    $sp.Dispose()
}
