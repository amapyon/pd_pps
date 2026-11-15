[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Port,
    [ValidateRange(1,300)][int]$WaitSeconds = 45,
    [ValidateRange(100,3000)][int]$IntervalMs = 300,
    [string]$LogPath = '.\M43A_session.txt',
    [switch]$Immediate
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$sp = New-Object System.IO.Ports.SerialPort($Port,115200,'None',8,'One')
$sp.NewLine = "`n"
$sp.ReadTimeout = 4000
$sp.WriteTimeout = 4000
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$rows = New-Object 'System.Collections.Generic.List[string]'
$baselineDropped = [long]0

function Send-Line([string]$Cmd) {
    $sp.Write($Cmd + "`n")
    $result = $sp.ReadLine().Trim()
    $prefix = $Cmd.Split(' ')[0]
    if (-not $result.StartsWith($prefix + ' ')) {
        throw ("Unexpected $Cmd -> $result")
    }
    return $result
}
function Record([string]$Line) {
    $s = ('{0:HH:mm:ss.fff} +{1}ms {2}' -f (Get-Date), $sw.ElapsedMilliseconds, $Line)
    $rows.Add($s)
    Write-Host $s
}
function Has-Field([string]$Line, [string]$Field, [string]$Value) {
    # Anchor to whitespace boundaries so 'a=1' never matches 'ma=1'.
    return ($Line -match ('(?:^|\s)' + [regex]::Escape($Field) + '=' + [regex]::Escape($Value) + '(?:\s|$)'))
}
try {
    $sp.Open()
    Start-Sleep -Milliseconds 350
    $sp.DiscardInBuffer()
    $sp.Write("PING`n")
    $pong = $sp.ReadLine().Trim()
    if ($pong -ne 'PONG M43A') { throw ('Expected PONG M43A, received ' + $pong) }
    Record $pong

    if (-not $Immediate) {
        $stat0 = Send-Line 'STAT'
        $stop0 = Send-Line 'PDSTOP'
        $info0 = Send-Line 'PDTINFO'
        Record ('BASELINE ' + $stat0)
        Record ('BASELINE ' + $stop0)
        Record ('BASELINE ' + $info0)
        if ($info0 -match '(?:^|\s)dropped=(\d+)(?:\s|$)') {
            $baselineDropped = [long]$Matches[1]
        }
        if ((Has-Field $stat0 'a' '1') -or (Has-Field $stop0 'active' '1')) {
            throw 'PD cable is already attached. Disconnect it, wait for a=0, then rerun. Use -Immediate to snapshot an existing session.'
        }
        Record 'Monitoring for a NEW PD attachment. Connect the PD charger now.'
        $start = $sw.ElapsedMilliseconds
        $seenNewAttach = $false
        $foundNewFailure = $false
        while (($sw.ElapsedMilliseconds - $start) -lt ($WaitSeconds * 1000)) {
            $begin = $sw.ElapsedMilliseconds
            $stat = Send-Line 'STAT'
            $stop = Send-Line 'PDSTOP'
            Record $stat
            Record $stop
            if ((Has-Field $stat 'a' '1') -or (Has-Field $stop 'active' '1')) {
                if (-not $seenNewAttach) { Record 'NEW PD ATTACH observed.' }
                $seenNewAttach = $true
            }
            # M43A keeps found=1 after detachment. Only a fresh, active
            # attachment may trigger this capture. Never trigger on stale data.
            if ($seenNewAttach -and (Has-Field $stop 'active' '1') -and
                ((Has-Field $stop 'found' '1') -or (Has-Field $stat 'pe' '6'))) {
                $foundNewFailure = $true
                Record 'NEW PD FAILURE observed; capturing live trace.'
                break
            }
            $delay = $IntervalMs - [int]($sw.ElapsedMilliseconds - $begin)
            if ($delay -gt 0) { Start-Sleep -Milliseconds $delay }
        }
        if (-not $foundNewFailure) {
            Record 'No new PD failure detected before timeout; taking snapshot anyway.'
            if (-not $seenNewAttach) {
                Record 'WARNING: No new PD attach observed. Check charger and VBUS.'
            }
        }
    }
    Record (Send-Line 'STAT')
    Record (Send-Line 'PDSTOP')
    Record (Send-Line 'PDERR')
    Record (Send-Line 'PDSTAB')
    Record (Send-Line 'VBINFO')
    Record (Send-Line 'INA')
    $info = Send-Line 'PDTINFO'
    Record $info
    if ($info -notmatch '(?:^|\s)count=(\d+)(?:\s|$)') {
        throw ('Missing trace count: ' + $info)
    }
    $count = [int]$Matches[1]
    if ($count -lt 0 -or $count -gt 127) { throw ('Invalid trace count: ' + $count) }
    if ($info -match '(?:^|\s)dropped=(\d+)(?:\s|$)') {
        $dropped = [long]$Matches[1]
        $newDrops = $dropped - $baselineDropped
        if ($Immediate) {
            if ($dropped -gt 0) { Record 'NOTE: dropped counter is cumulative; session loss cannot be determined in -Immediate mode.' }
        } elseif ($newDrops -gt 0) {
            Record ("WARNING: $newDrops new trace records were dropped during this capture; earliest buffered records are retained.")
        }
    }
    if ($count -eq 0) { Record 'WARNING: Trace FIFO is empty. It may have been drained after physical detach.' }
    for ($i = 0; $i -lt $count; $i++) {
        $line = Send-Line ('PDTRACE ' + $i)
        Record $line
        if ($line -match '(?:^|\s)none=1(?:\s|$)') {
            Record 'WARNING: Trace index unavailable; buffer may have been drained.'
        }
    }
    # Pre-stop ADC snapshot survives live ring overwrite, even while still attached.
    $vb = Send-Line 'VBINFO'
    Record $vb
    if ($vb -match '(?:^|\s)stop_count=(\d+)(?:\s|$)') {
        $vcount = [int]$Matches[1]
        if ($vcount -gt 40) { throw ('Invalid VBLOG count: ' + $vcount) }
        for ($i = 0; $i -lt $vcount; $i++) {
            Record (Send-Line ('VBLOG ' + $i))
        }
    }
    Record (Send-Line 'INA')
} finally {
    try {
        $parent = Split-Path -Parent $LogPath
        if ($parent -and -not (Test-Path -LiteralPath $parent)) {
            New-Item -ItemType Directory -Path $parent -Force | Out-Null
        }
        $rows | Set-Content -LiteralPath $LogPath -Encoding UTF8
        Write-Host ('Saved: ' + $LogPath)
    } finally {
        if ($sp.IsOpen) { $sp.Close() }
        $sp.Dispose()
    }
}
