[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Port,
    [ValidateRange(0,300)][int]$WaitSeconds = 40,
    [ValidateRange(100,3000)][int]$IntervalMs = 300,
    [string]$LogPath = '.\M41_trace.txt',
    [switch]$Immediate
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$sp = New-Object System.IO.Ports.SerialPort($Port,115200,'None',8,'One')
$sp.NewLine = "`n"; $sp.ReadTimeout = 4000; $sp.WriteTimeout = 4000
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$rows = New-Object 'System.Collections.Generic.List[string]'
function Send-Line([string]$Cmd) {
    $sp.Write($Cmd + "`n")
    $result = $sp.ReadLine().Trim()
    $prefix = $Cmd.Split(' ')[0]
    if (-not $result.StartsWith($prefix + ' ')) { throw ("Unexpected $Cmd -> $result") }
    return $result
}
function Record([string]$Line) {
    $s = ('{0:HH:mm:ss.fff} +{1}ms {2}' -f (Get-Date), $sw.ElapsedMilliseconds, $Line)
    $rows.Add($s)
    Write-Host $s
}
try {
    $sp.Open()
    Start-Sleep -Milliseconds 350
    $sp.DiscardInBuffer()
    $sp.Write("PING`n")
    $pong = $sp.ReadLine().Trim()
    if ($pong -ne 'PONG M41') { throw ('Expected PONG M41, received ' + $pong) }
    Record $pong
    if (!$Immediate) {
        Write-Host 'Monitoring for first PD failure. Plug the PD charger in now if it is disconnected.'
        $start = $sw.ElapsedMilliseconds
        $found = $false
        while (($sw.ElapsedMilliseconds - $start) -lt ($WaitSeconds * 1000)) {
            $begin = $sw.ElapsedMilliseconds
            $stat = Send-Line 'STAT'
            $stop = Send-Line 'PDSTOP'
            Record $stat
            Record $stop
            if ($stop -match 'found=1') { $found = $true; break }
            if ($stat -match 'pe=6(\s|$)') { $found = $true; break }
            $delay = $IntervalMs - [int]($sw.ElapsedMilliseconds - $begin)
            if ($delay -gt 0) { Start-Sleep -Milliseconds $delay }
        }
        if (-not $found) { Record 'No PD failure detected within the polling window; taking a snapshot anyway.' }
    }
    Record (Send-Line 'PDERR')
    Record (Send-Line 'PDSTAB')
    $info = Send-Line 'PDTINFO'
    Record $info
    if ($info -notmatch 'count=(\d+)') { throw ('Missing trace count: ' + $info) }
    $count = [int]$Matches[1]
    if ($count -lt 0 -or $count -gt 127) { throw ('Invalid trace count: ' + $count) }
    if ($info -match 'dropped=(\d+)' -and [long]$Matches[1] -gt 0) {
        Record 'WARNING: PD trace buffer overflow; newest events may be absent.'
    }
    for ($i=0; $i -lt $count; $i++) {
        $line = Send-Line ('PDTRACE ' + $i)
        Record $line
        if ($line -match 'none=1') { Record 'WARNING: trace index unavailable (device disconnected or trace drained).' }
    }
} finally {
    try {
        $parent = Split-Path -Parent $LogPath
        if ($parent -and !(Test-Path -LiteralPath $parent)) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
        $rows | Set-Content -LiteralPath $LogPath -Encoding UTF8
        Write-Host ('Saved: ' + $LogPath)
    } finally {
        if ($sp.IsOpen) { $sp.Close() }
        $sp.Dispose()
    }
}
