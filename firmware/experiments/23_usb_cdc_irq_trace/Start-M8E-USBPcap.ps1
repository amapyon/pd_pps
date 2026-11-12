#requires -Version 5.1
<#
Windows USBPcap capture for one explicitly chosen device address and root hub.
Does not install drivers and does not auto-select a capture interface.
This is a privileged, privacy-sensitive capture; inspect arguments before running.
Run in a separate elevated PowerShell window and press q in USBPcapCMD to stop.
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)] [ValidatePattern('^\\\\\.\\USBPcap[1-9][0-9]*$')] [string]$RootHub,
  [Parameter(Mandatory=$true)] [ValidateRange(1,127)] [int]$DeviceAddress,
  [string]$WorkspaceRoot = 'C:\pj\pd_pps',
  [string]$UsbPcapCmd = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (-not $UsbPcapCmd) {
    $candidates = @('C:\Program Files\USBPcap\USBPcapCMD.exe', 'C:\Program Files\Wireshark\extcap\USBPcapCMD.exe')
    $UsbPcapCmd = @($candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1)
    if ($UsbPcapCmd -is [array]) { $UsbPcapCmd = if ($UsbPcapCmd.Count -gt 0) { $UsbPcapCmd[0] } else { '' } }
}
if (-not $UsbPcapCmd -or -not (Test-Path -LiteralPath $UsbPcapCmd -PathType Leaf)) {
    throw 'USBPcapCMD.exe not found. Install USBPcap via trusted Wireshark installer and locate the command.'
}
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run from an elevated PowerShell window; USB capture requires sufficient rights.'
}
$folder = Join-Path $WorkspaceRoot 'diagnostics\m8e_irqtrace'
New-Item -ItemType Directory -Path $folder -Force | Out-Null
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$file=Join-Path $folder ('M8E_USB_URB_'+$stamp+'.pcap')
Write-Host "Root hub: $RootHub | device address: $DeviceAddress"
Write-Host "Capture file: $file"
Write-Warning 'USBPcap captures URBs for the selected USB device. PCAP may contain sensitive device data.'
Write-Host 'Start capture now, run Test-M8E-Stall-Isolation.ps1 in another window, then press q to stop capture.'
& $UsbPcapCmd -d $RootHub --devices ([string]$DeviceAddress) -o $file --inject-descriptors
$code=$LASTEXITCODE
Write-Host "USBPcap exit code: $code"
if (Test-Path -LiteralPath $file -PathType Leaf) { Write-Host "Capture: $file" }
if ($code -ne 0) { throw "USBPcapCMD exit code: $code" }
