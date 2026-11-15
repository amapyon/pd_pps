[CmdletBinding()]
param([string]$Port = '')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ([string]::IsNullOrWhiteSpace($Port)) {
    $ports = @(Get-PnpDevice -Class Ports -ErrorAction Stop |
        Where-Object { $_.InstanceId -match 'VID_1209&PID_D035' -and $_.Status -eq 'OK' })
    $m28 = @($ports | Where-Object { $_.InstanceId -match '\\008($|\\)' })
    if ($m28.Count -ne 1) {
        $ports | Format-Table FriendlyName, InstanceId, Status -AutoSize
        throw 'Unable to auto-select M28 serial 008; supply -Port COMx explicitly.'
    }
    if ($m28[0].FriendlyName -notmatch '(COM\d+)') { throw 'No COM number in M28 FriendlyName' }
    $Port = $Matches[1]
}
Write-Host "M28 read-only CDC test on $Port (no PPS writes)"
$serial = New-Object System.IO.Ports.SerialPort
$serial.PortName = $Port
$serial.BaudRate = 115200
$serial.Parity = [System.IO.Ports.Parity]::None
$serial.DataBits = 8
$serial.StopBits = [System.IO.Ports.StopBits]::One
$serial.NewLine = "`n"
$serial.ReadTimeout = 2500
$serial.WriteTimeout = 1000
$serial.DtrEnable = $true
$serial.RtsEnable = $false
try {
    $serial.Open()
    $serial.DiscardInBuffer()
    function Ask([string]$command) {
        if ($command -match '^SET_|^OUTPUT_') { throw "Output mutation forbidden: $command" }
        $serial.Write($command + "`n")
        $reply = $serial.ReadLine().TrimEnd("`r")
        Write-Host "$command : $reply"
        return $reply
    }
    $info = Ask 'INFO'
    if ($info -notmatch 'firmware=m28_pd_usb_cdc protocol=7') {
        throw "Unexpected firmware or protocol: $info"
    }
    $null = Ask 'HEALTH'
    $caps = Ask 'GET_CAPS'
    if ($caps -match 'CAPS status=ok count=(\d+)') {
        $count = [int]$Matches[1]
        if ($count -lt 1 -or $count -gt 7) { throw "Invalid caps count: $count" }
        for ($i=1; $i -le $count; ++$i) { $null = Ask "GET_CAP $i" }
    }
    $null = Ask 'STATUS'
    $null = Ask 'MEASURE'
    Write-Host 'M28 read-only CDC command exchange PASS (not PD/PPS timing or accuracy certification).'
} finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
