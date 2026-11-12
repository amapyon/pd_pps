#requires -Version 5.1
<#
M8F CDC continuous measurements with bounded reconnect and honest gap logging.
Only PING and MEASURE (idempotent) are retried. Never writes to hardware power.
No simulator values. Wait indefinitely for reconnection by default (MaxReconnectSeconds=0).
#>
[CmdletBinding()]
param(
    [ValidateSet('MeasureOnly','PingOnly')]
    [string]$Mode = 'MeasureOnly',
    [ValidateRange(1,1000000)]
    [int]$Count = 100,
    [ValidateRange(0,60000)]
    [int]$IntervalMs = 250,
    [ValidateRange(1000,30000)]
    [int]$TimeoutMs = 4500,
    [ValidateRange(0,86400)]
    [int]$MaxReconnectSeconds = 0,
    [string]$Port = '',
    [string]$OutputDirectory = 'C:\pj\pd_pps\diagnostics\m8f_recovery'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$expectedInfo = 'INFO board=CH32X035F8U6 firmware=m8f_usb_cdc_auto_recover protocol=6'
$measurePattern = '^MEASURE status=ok voltage_mV=(\d+) current_mA=(-?\d+)$'
$notConfiguredPattern = '^MEASURE status=(error|unavailable) .*voltage_mV=NA current_mA=NA$'

function Find-DevicePort {
    $candidates = @()
    try {
        $devices = @(Get-PnpDevice -Class Ports -PresentOnly -ErrorAction Stop |
            Where-Object { $_.InstanceId -match 'VID_1209&PID_D035' -and $_.Status -eq 'OK' })
    } catch {
        throw "PnP port enumeration failed: $($_.Exception.Message)"
    }
    $openedNames = @([System.IO.Ports.SerialPort]::GetPortNames())
    foreach ($dev in $devices) {
        if ($dev.FriendlyName -match '\((COM\d+)\)') {
            $name = $Matches[1]
            if ($openedNames -contains $name) { $candidates += $name }
        }
    }
    $candidates = @($candidates | Select-Object -Unique)
    if ($candidates.Count -eq 0) { return $null }
    if ($candidates.Count -gt 1) {
        if (-not [string]::IsNullOrWhiteSpace($Port) -and $candidates -contains $Port) {
            return $Port
        }
        throw "Multiple matching VID_1209 PID_D035 ports: $($candidates -join ', '). Specify -Port."
    }
    return $candidates[0]
}

function Request-Line([System.IO.Ports.SerialPort]$serialPort, [string]$command) {
    $serialPort.Write($command + "`n")
    return $serialPort.ReadLine().TrimEnd([char]13)
}

function Open-VerifiedPort([string]$portName) {
    $sp = [System.IO.Ports.SerialPort]::new($portName,115200,'None',8,'One')
    $sp.ReadTimeout = $TimeoutMs
    $sp.WriteTimeout = $TimeoutMs
    $sp.Handshake = [System.IO.Ports.Handshake]::None
    $sp.DtrEnable = $true
    $sp.RtsEnable = $true
    $sp.NewLine = "`n"
    try {
        $sp.Open()
        Start-Sleep -Milliseconds 300
        $sp.DiscardInBuffer()
        $info = Request-Line $sp 'INFO'
        if ($info -ne $expectedInfo) {
            throw "Unexpected firmware identity on ${portName}: [$info]"
        }
        $health = Request-Line $sp 'HEALTH'
        if ($health -notmatch '^HEALTH bus_resets=\d+ tx_timeouts=\d+ reconnects=\d+$') {
            throw "Unexpected HEALTH: [$health]"
        }
        Write-Host "CONNECTED: $portName; $health"
        return $sp
    } catch {
        if ($sp.IsOpen) { $sp.Close() }
        $sp.Dispose()
        throw
    }
}

function Close-Port([System.IO.Ports.SerialPort]$sp) {
    if ($null -ne $sp) {
        try { if ($sp.IsOpen) { $sp.Close() } } catch { }
        try { $sp.Dispose() } catch { }
    }
}

if (-not (Test-Path -LiteralPath $OutputDirectory -PathType Container)) {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
}
$stamp = (Get-Date).ToString('yyyyMMdd-HHmmss-fff')
$csvPath = Join-Path $OutputDirectory "M8F_${Mode}_${stamp}.csv"
$logPath = Join-Path $OutputDirectory "M8F_${Mode}_${stamp}.txt"
$writer = $null
$serial = $null
$completed = 0
$reconnects = 0
$outage = $null
$lastNotice = $null
$lastPort = ''
$lastError = ''
try {
    $writer = [System.IO.StreamWriter]::new($csvPath, $false, [System.Text.UTF8Encoding]::new($true))
    $writer.AutoFlush = $true
    $writer.WriteLine('timestamp,sequence,port,mode,voltage_mV,current_mA,response')
    Write-Host "START: $Mode; Count=$Count; IntervalMs=$IntervalMs; MaxReconnectSeconds=$MaxReconnectSeconds"
    while ($completed -lt $Count) {
        if ($null -eq $serial) {
            if ($null -eq $outage) { $outage = Get-Date }
            if ($MaxReconnectSeconds -gt 0 -and
                ((Get-Date) - $outage).TotalSeconds -ge $MaxReconnectSeconds) {
                throw "Reconnect timed out after $MaxReconnectSeconds seconds. Last error: $lastError"
            }
            if ($null -eq $lastNotice -or ((Get-Date) - $lastNotice).TotalSeconds -ge 10) {
                Write-Host "WAITING: reconnecting VID_1209 PID_D035, completed=$completed/$Count"
                $lastNotice = Get-Date
            }
            $candidate = Find-DevicePort
            if ($null -eq $candidate) {
                Start-Sleep -Milliseconds 500
                continue
            }
            try {
                $serial = Open-VerifiedPort $candidate
                $lastPort = $candidate
                $outage = $null
                $lastNotice = $null
            } catch {
                $lastError = $_.Exception.Message
                if ($lastError -like 'Unexpected firmware identity*') { throw }
                if ($lastError -like 'Unexpected HEALTH:*') { throw }
                Start-Sleep -Milliseconds 500
                continue
            }
        }
        try {
            $command = if ($Mode -eq 'PingOnly') { 'PING' } else { 'MEASURE' }
            $reply = Request-Line $serial $command
            $voltage = 'NA'
            $current = 'NA'
            if ($Mode -eq 'PingOnly') {
                if ($reply -ne 'PONG') { throw "Unexpected PING: [$reply]" }
            } else {
                if ($reply -match $measurePattern) {
                    $voltage = $Matches[1]
                    $current = $Matches[2]
                } elseif ($reply -match $notConfiguredPattern) {
                    throw "Measurement failed (not a valid sample): [$reply]"
                } else {
                    throw "Unexpected MEASURE reply: [$reply]"
                }
            }
            $completed++
            $nowText = (Get-Date).ToString('yyyy-MM-ddTHH:mm:ss.fffzzz')
            $writer.WriteLine("${nowText},${completed},${lastPort},${Mode},${voltage},${current},${reply}")
            Write-Host ("{0}/{1} {2}: {3}" -f $completed,$Count,$lastPort,$reply)
            if ($completed -lt $Count -and $IntervalMs -gt 0) {
                Start-Sleep -Milliseconds $IntervalMs
            }
        } catch {
            $lastError = $_.Exception.Message
            if ($lastError -like 'Measurement failed*') { throw }
            $reconnects++
            $eventLine = "$(Get-Date -Format o) DISCONNECTED at sample $($completed+1): $lastError"
            Write-Warning $eventLine
            Add-Content -LiteralPath $logPath -Value $eventLine -Encoding UTF8
            Close-Port $serial
            $serial = $null
            $outage = Get-Date
        }
    }
    $summary = "PASS: $completed/$Count; disconnects=$reconnects; csv=$csvPath"
    Write-Host $summary
    Add-Content -LiteralPath $logPath -Value $summary -Encoding UTF8
} catch {
    $failure = $_.Exception.Message
    Write-Warning "INCOMPLETE: $completed/$Count; $failure"
    Add-Content -LiteralPath $logPath -Value "INCOMPLETE $completed/$Count; $failure" -Encoding UTF8
    throw
} finally {
    Close-Port $serial
    if ($null -ne $writer) { $writer.Dispose() }
    Write-Host "CSV: $csvPath"
    Write-Host "LOG: $logPath"
}
