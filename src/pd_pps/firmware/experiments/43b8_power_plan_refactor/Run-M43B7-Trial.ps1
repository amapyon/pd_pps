[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateSet('PD','CONTROL','SRAM')][string]$Phase,
    [Parameter(Mandatory=$true)][ValidateSet('R01_OFF_CC1','R02_ON_CC1_IDLE')][string]$Trial
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$port=$null; $writer=$null; $stream=$null
try {
    $root='C:\pj\pd_pps'
    $project=Join-Path $root 'src\pd_pps\firmware\experiments\43b7_p2_session_refactor'
    $dir=Join-Path $root 'diagnostics\m43b7_p2_session_refactor\logs'
    $link=Join-Path $root 'src\pd_pps\ch32fun\minichlink\minichlink.exe'
    $id='M43B7_'+$Trial
    $isOn=($Trial -eq 'R02_ON_CC1_IDLE')
    if (-not (Test-Path -LiteralPath $project -PathType Container)) { throw 'Project not installed' }
    if ($Phase -eq 'SRAM') {
        $reader=Join-Path $project 'Read-M43B7-SRAM.ps1'
        if (-not (Test-Path -LiteralPath $reader -PathType Leaf)) { throw "Missing: $reader" }
        Write-Host 'Before SRAM: PD disconnected, PD terminal closed, MCU power kept ON.'
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $reader -Root $root
        if ($LASTEXITCODE -ne 0) { throw "SRAM reader failed: $LASTEXITCODE" }
        return
    }
    if ($Phase -eq 'PD' -and -not (Test-Path -LiteralPath $link -PathType Leaf)) {
        throw "Missing WCH-LinkE: $link"
    }
    New-Item -ItemType Directory -Path $dir -Force -ErrorAction Stop | Out-Null
    if ($Phase -eq 'PD') {
        $log=Join-Path $dir ($id+'_pd.log')
        if (Test-Path -LiteralPath $log) { throw "Log exists: $log" }
        Write-Host "TEST: $id"
        Write-Host "PD log: $log"
        Write-Host 'PD charger must be detached. Confirm repeated PD17: alive, charger detached.'
        Write-Host 'Leave this window running until after CONTROL finishes and TRACE/VBUS END is printed.'
        & $link -T | Tee-Object -FilePath $log
        return
    }
    $log=Join-Path $dir ($id+'_control.log')
    if (Test-Path -LiteralPath $log) { throw "Log exists: $log" }
    if ($isOn) {
        if ('COM16' -notin @([IO.Ports.SerialPort]::GetPortNames())) { throw 'COM16 missing' }
    } else {
        $cdc=@(Get-PnpDevice -PresentOnly -ErrorAction Stop |
            Where-Object { $_.InstanceId -like 'USB\VID_1209&PID_D035*' })
        if ($cdc.Count -gt 0) { throw 'CH32X035 CDC device still connected; OFF test blocked' }
    }
    $answer=Read-Host ('Verify WCH-LinkE connected, PD charger detached, no external load, CC1, PC CDC '+$(if($isOn){'ON'}else{'OFF'})+'. Enter READY')
    if ($answer -cne 'READY') { throw 'Preconditions not confirmed' }
    $stream=[IO.File]::Open($log,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    $writer=[IO.StreamWriter]::new($stream,[Text.UTF8Encoding]::new($false))
    $writer.AutoFlush=$true
    $writer.WriteLine('TEST '+$id+' MODE='+$(if($isOn){'USB_ON_IDLE'}else{'USB_OFF'}))
    $writer.WriteLine('START '+(Get-Date -Format o))
    if ($isOn) {
        $port=[IO.Ports.SerialPort]::new('COM16',115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
        $port.NewLine="`n"; $port.ReadTimeout=1500; $port.WriteTimeout=1500
        $port.DtrEnable=$true; $port.Open()
        Start-Sleep -Milliseconds 300; $port.DiscardInBuffer()
        $port.WriteLine('STAT');$reply=$port.ReadLine().Trim()
        $writer.WriteLine('PRECHECK '+$reply)
        if ($reply -notmatch '^STAT a=0 pe=0 ') { throw "PD attached at PRECHECK: $reply" }
    }
    $sw=[Diagnostics.Stopwatch]::StartNew()
    $attachedPrompt=$false;$detachedPrompt=$false;$quietStart=0.0
    Write-Host "START $id"
    Write-Host 'PD charger: KEEP DETACHED until the CONNECT instruction.'
    while ($sw.Elapsed.TotalSeconds -lt 55.0) {
        $seconds=$sw.Elapsed.TotalSeconds
        if (-not $attachedPrompt -and $seconds -ge 5.0) {
            if ($isOn) {
                $port.WriteLine('STAT');$reply=$port.ReadLine().Trim()
                $writer.WriteLine(('{0:F3} PRE_ATTACH {1}' -f $sw.Elapsed.TotalMilliseconds,$reply))
                if ($reply -notmatch '^STAT a=0 pe=0 ') { throw "Early attach: $reply" }
            }
            $attachedPrompt=$true
            $quietStart=$sw.Elapsed.TotalMilliseconds
            $writer.WriteLine(('{0:F3} MARKER ATTACH_PROMPT {1}' -f $quietStart,(Get-Date -Format o)))
            Write-Host ''
            Write-Host 'CONNECT PD CHARGER NOW (CC1)' -ForegroundColor Cyan
            Write-Host 'Do not unplug until the DISCONNECT prompt.'
            Write-Host ''
        }
        if (-not $detachedPrompt -and $seconds -ge 38.0) {
            $detachedPrompt=$true
            $writer.WriteLine(('{0:F3} MARKER DETACH_PROMPT {1}' -f $sw.Elapsed.TotalMilliseconds,(Get-Date -Format o)))
            Write-Host ''
            Write-Host 'DISCONNECT PD CHARGER NOW' -ForegroundColor Yellow
            Write-Host 'Keep WCH-LinkE and MCU power connected.'
            Write-Host ''
        }
        Start-Sleep -Milliseconds 10
    }
    $check=Read-Host 'PD charger physically disconnected? Enter DETACHED'
    if ($check -cne 'DETACHED') { throw 'Physical PD detach not confirmed' }
    $writer.WriteLine(('{0:F3} MARKER USER_DETACHED {1}' -f $sw.Elapsed.TotalMilliseconds,(Get-Date -Format o)))
    Start-Sleep -Seconds 2
    if ($isOn) {
        $writer.WriteLine(('{0:F3} MARKER QUIET_END' -f $sw.Elapsed.TotalMilliseconds))
        $port.WriteLine('STAT');$reply=$port.ReadLine().Trim()
        $writer.WriteLine('POSTCHECK '+$reply)
        if ($reply -notmatch '^STAT a=0 pe=0 ') { throw "Not detached: $reply" }
        $port.WriteLine('PING');$pong=$port.ReadLine().Trim()
        $writer.WriteLine('POSTPING '+$pong)
        if ($pong -ne 'PONG M43A') { throw "PONG mismatch: $pong" }
        $port.WriteLine('COUNT');$count=$port.ReadLine().Trim()
        $writer.WriteLine('POSTCOUNT '+$count)
        if ($count -notmatch '^COUNT set=') { throw "COUNT mismatch: $count" }
    }
    $writer.WriteLine(('SUMMARY elapsed_seconds={0:F3} usb_on={1} physical_detach=True' -f $sw.Elapsed.TotalSeconds,$isOn))
    Write-Host 'CONTROL COMPLETED. Confirm PD TRACE END and VBUS END; stop terminal before SRAM.' -ForegroundColor Green
    Write-Host "Log: $log"
} catch {
    if ($null -ne $writer) { $writer.WriteLine('ERROR '+(Get-Date -Format o)+' '+$_.Exception.Message) }
    Write-Host ('STOPPED: '+$_.Exception.Message) -ForegroundColor Red
    throw
} finally {
    if ($null -ne $port) { if ($port.IsOpen) { $port.Close() }; $port.Dispose() }
    if ($null -ne $writer) { $writer.Dispose() } elseif ($null -ne $stream) { $stream.Dispose() }
}
