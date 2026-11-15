[CmdletBinding()]
param([string]$Port='COM10')
$ErrorActionPreference='Stop'
$serial=New-Object System.IO.Ports.SerialPort($Port,115200,'None',8,'One')
$serial.NewLine="`n"; $serial.ReadTimeout=2500; $serial.WriteTimeout=2500
try {
    $serial.Open(); Start-Sleep -Milliseconds 500; $serial.DiscardInBuffer()
    $serial.Write("PHYDIAG`n")
    $line=$serial.ReadLine().Trim(); Write-Host $line
    if($line -notmatch '^PHYDIAG n=(\d+)') { throw "Unexpected response: $line" }
    $count=[int]$Matches[1]
    if($count -lt 0 -or $count -gt 6) { throw "Invalid n=$count" }
    for($i=0;$i -lt $count;$i++) {
        foreach($prefix in @('S','R')) {
            $serial.Write("PHYDIAG $prefix$i`n")
            $line=$serial.ReadLine().Trim()
            if($line -notmatch '^(PS|PR) i=') { throw "Unexpected response: $line" }
            Write-Host $line
        }
    }
} finally { if($serial.IsOpen){$serial.Close()}; $serial.Dispose() }
