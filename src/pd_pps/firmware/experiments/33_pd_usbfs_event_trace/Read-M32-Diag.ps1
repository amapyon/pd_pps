[CmdletBinding()]
param([string]$Port='COM10')
$ErrorActionPreference='Stop'
$serial=New-Object System.IO.Ports.SerialPort($Port,115200,'None',8,'One')
$serial.NewLine="`n"
$serial.ReadTimeout=2500
$serial.WriteTimeout=2500
try {
    $serial.Open()
    Start-Sleep -Milliseconds 500
    $serial.DiscardInBuffer()
    $serial.Write("XDIAG`n")
    $summary=$serial.ReadLine().Trim()
    Write-Host $summary
    if($summary -notmatch '^XDIAG n=(\d+)') { throw "Unexpected response: $summary" }
    $count=[int]$Matches[1]
    if($count -lt 0 -or $count -gt 12) {throw "Invalid n=$count"}
    for($i=0;$i -lt $count;$i++) {
        foreach($command in @("XDIAG $i", "XDIAG $i R", "XDIAG $i I")) {
            $serial.Write("$command`n")
            Write-Host ($serial.ReadLine().Trim())
        }
    }
} finally {
    if($serial.IsOpen){$serial.Close()}
    $serial.Dispose()
}
