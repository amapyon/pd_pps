[CmdletBinding()]
param([string]$Port='COM10')
$ErrorActionPreference='Stop'
$serial = New-Object System.IO.Ports.SerialPort($Port,115200,'None',8,'One')
$serial.NewLine = "`n"
$serial.ReadTimeout = 2000
$serial.WriteTimeout = 2000
try {
    $serial.Open()
    Start-Sleep -Milliseconds 500
    $serial.DiscardInBuffer()
    $serial.Write("DIAG`n")
    $summary = $serial.ReadLine().Trim()
    Write-Host $summary
    if($summary -notmatch '^DIAG n=(\d+)') { throw "Unexpected response: $summary" }
    $count = [int]$Matches[1]
    if($count -lt 0 -or $count -gt 12) { throw "Invalid n=$count" }
    for($i=0;$i -lt $count;$i++) {
        $serial.Write("DIAG $i`n")
        Write-Host ($serial.ReadLine().Trim())
    }
} finally {
    if($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
