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
    foreach($command in @('TDIAG','TDIAG C','TDIAG P','TDIAG B')) {
        $serial.Write("$command`n")
        $line=$serial.ReadLine().Trim()
        Write-Host $line
        if($command -eq 'TDIAG') {
            if($line -notmatch '^TDIAG n=(\d+)') {throw "Unexpected response: $line"}
            $count=[int]$Matches[1]
            if($count -lt 0 -or $count -gt 40) {throw "Invalid n=$count"}
        }
    }
    for($i=0;$i -lt $count;$i++) {
        $serial.Write("TDIAG $i`n")
        Write-Host ($serial.ReadLine().Trim())
    }
} finally {
    if($serial.IsOpen){$serial.Close()}
    $serial.Dispose()
}
