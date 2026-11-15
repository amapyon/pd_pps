[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Port,
    [ValidateRange(5,600)][int]$Seconds = 30,
    [ValidateRange(250,10000)][int]$IntervalMs = 1000,
    [string]$LogPath = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$sp = New-Object System.IO.Ports.SerialPort($Port,115200,'None',8,'One')
$sp.NewLine = "`n"
$sp.ReadTimeout = 4000
$sp.WriteTimeout = 4000
try {
    $sp.Open()
    Start-Sleep -Milliseconds 350
    $sp.DiscardInBuffer()
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt $Seconds) {
        $cycleStart = $watch.ElapsedMilliseconds
        foreach ($cmd in @('STAT','PDSTOP','PDSTAB','PDERR')) {
            $sp.Write($cmd + "`n")
            $reply = $sp.ReadLine().Trim()
            if (!$reply.StartsWith($cmd + ' ')) { throw ('Unexpected ' + $cmd + ': ' + $reply) }
            $msg = ('{0:HH:mm:ss.fff} +{1}ms {2}' -f (Get-Date), $watch.ElapsedMilliseconds, $reply)
            Write-Host $msg
            if ($LogPath) { Add-Content -LiteralPath $LogPath -Value $msg -Encoding UTF8 }
        }
        $remain = $IntervalMs - [int]($watch.ElapsedMilliseconds - $cycleStart)
        if ($remain -gt 0) { Start-Sleep -Milliseconds $remain }
    }
} finally {
    if ($sp.IsOpen) { $sp.Close() }
    $sp.Dispose()
}
