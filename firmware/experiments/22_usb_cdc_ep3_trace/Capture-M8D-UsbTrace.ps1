<#
M8D USB EP3 trace diagnostic firmware capture: find minichlink.exe even if it is
not on PATH and read CH32X035 20-KiB SRAM. Archives the exact current ELF/source.

Read-only command to target (minichlink -r). Writes diagnostics under the workspace;
never builds, flashes, edits firmware/sources, changes Git or requests reset.
Attaching a debugger may nevertheless halt or reset the target.
Windows PowerShell 5.1 compatible.
#>
[CmdletBinding()]
param(
    [string]$WorkspaceRoot = 'C:\pj\pd_pps',
    [string]$Port = 'COM9',
    [ValidateSet('Healthy', 'Failed', 'Unknown')]
    [string]$State = 'Unknown',
    [string]$MinichlinkPath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Join-Path $WorkspaceRoot 'src\pd_pps'
$m8 = Join-Path $repo 'firmware\experiments\22_usb_cdc_ep3_trace'
$elf = Join-Path $m8 'm8d_usb_cdc_ep3_trace.elf'
$src = Join-Path $m8 'm8d_usb_cdc_ep3_trace.c'

foreach ($file in @($elf, $src)) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        throw "Missing: $file"
    }
}

# Source hash check validates the packaged M8D diagnostic main, not experiment 21.
# The local ELF must contain the m8d_usb_trace symbol and be newer than source and trace driver.
# A successful check does not prove the precise binary on the target.
$expectedSrc = '8A1B49FF6D5AB74D56FB020B8A8F1FC72047A5A14A31D065C14887EB6C89C19E'
$elfHash = (Get-FileHash -LiteralPath $elf -Algorithm SHA256).Hash.ToUpperInvariant()
$srcHash = (Get-FileHash -LiteralPath $src -Algorithm SHA256).Hash.ToUpperInvariant()
if ($srcHash -ne $expectedSrc) {
    throw "M8D source differs from diagnostic package. No RAM read attempted.`nExpected: $expectedSrc`nActual:   $srcHash"
}
$elfInfo = Get-Item -LiteralPath $elf
$srcInfo = Get-Item -LiteralPath $src
$traceDriver = Join-Path $m8 'm8d_trace.c'
if (-not (Test-Path -LiteralPath $traceDriver -PathType Leaf)) { throw "Missing: $traceDriver" }
$traceInfo = Get-Item -LiteralPath $traceDriver
$map = Join-Path $m8 'm8d_usb_cdc_ep3_trace.map'
if (-not (Test-Path -LiteralPath $map -PathType Leaf)) {
    throw 'Missing M8D MAP file. Build must produce a .map before SRAM capture.'
}
$elfData = [System.IO.File]::ReadAllBytes($elf)
if ($elfData.Length -lt 64 -or $elfData[0] -ne 0x7f -or
    $elfData[1] -ne 0x45 -or $elfData[2] -ne 0x4c -or
    $elfData[3] -ne 0x46) {
    throw 'M8D ELF does not have an ELF header. No RAM read attempted.'
}
if (-not [System.Text.Encoding]::ASCII.GetString($elfData).Contains('m8d_usb_trace')) {
    throw 'M8D ELF lacks m8d_usb_trace symbol. The local M8D ELF may be stale. No RAM read attempted.'
}
if ($elfInfo.LastWriteTimeUtc -lt $srcInfo.LastWriteTimeUtc -or
    $elfInfo.LastWriteTimeUtc -lt $traceInfo.LastWriteTimeUtc) {
    throw 'M8D ELF is older than its source and may be stale. Do not overwrite a failed running device before capture. No RAM read attempted.'
}

# The prior flash command found minichlink through the workspace's toolchain.
# A new PowerShell session may not have that tool directory on its PATH.
$mini = $null
if (-not [string]::IsNullOrWhiteSpace($MinichlinkPath)) {
    if (-not (Test-Path -LiteralPath $MinichlinkPath -PathType Leaf)) {
        throw "Specified minichlink.exe does not exist: $MinichlinkPath"
    }
    $mini = (Resolve-Path -LiteralPath $MinichlinkPath).ProviderPath
}
else {
    foreach ($name in @('minichlink.exe', 'minichlink')) {
        $cmd = Get-Command $name -CommandType Application -ErrorAction SilentlyContinue
        if ($null -ne $cmd) { $mini = $cmd.Source; break }
    }
    if ($null -eq $mini) {
        $fastCandidates = @(
            (Join-Path $repo 'ch32fun\minichlink\minichlink.exe'),
            (Join-Path $WorkspaceRoot 'tools\minichlink.exe'),
            (Join-Path $repo 'dev-workspace\minichlink.exe')
        )
        foreach ($candidate in $fastCandidates) {
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                $mini = $candidate
                break
            }
        }
    }
    if ($null -eq $mini) {
        foreach ($base in @(
            (Join-Path $WorkspaceRoot 'tools'),
            (Join-Path $repo 'ch32fun'),
            (Join-Path $repo 'dev-workspace'),
            'C:\msys64\mingw64\bin'
        )) {
            if (-not (Test-Path -LiteralPath $base -PathType Container)) { continue }
            $found = @(Get-ChildItem -LiteralPath $base -Filter 'minichlink.exe' -File -Recurse -ErrorAction SilentlyContinue |
                       Select-Object -First 1)
            if ($found.Count -gt 0) { $mini = $found[0].FullName; break }
        }
    }
}
if ([string]::IsNullOrWhiteSpace($mini)) {
    throw @'
minichlink.exe was not found. No RAM read was attempted.
Locate the executable used by the successful Flash step and run this script
with -MinichlinkPath "C:\full\path\to\minichlink.exe".
Example search:
  Get-ChildItem 'C:\pj\pd_pps' -Filter minichlink.exe -File -Recurse -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
'@
}

$diagnostics = Join-Path $WorkspaceRoot 'diagnostics'
New-Item -ItemType Directory -Path $diagnostics -Force | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$folder = Join-Path $diagnostics "M8D-EP3-trace-$State-$stamp"
New-Item -ItemType Directory -Path $folder -ErrorAction Stop | Out-Null
$ram = Join-Path $folder 'sram-20kb.bin'
$log = Join-Path $folder 'minichlink-read.log'
$zip = "$folder.zip"

Copy-Item -LiteralPath $elf -Destination (Join-Path $folder 'm8d_usb_cdc_ep3_trace.elf')
Copy-Item -LiteralPath $src -Destination (Join-Path $folder 'm8d_usb_cdc_ep3_trace.c')
Copy-Item -LiteralPath $traceDriver -Destination (Join-Path $folder 'm8d_trace.c')
Copy-Item -LiteralPath (Join-Path $m8 'm8d_trace.h') -Destination (Join-Path $folder 'm8d_trace.h')
Copy-Item -LiteralPath $map -Destination (Join-Path $folder 'm8d_usb_cdc_ep3_trace.map')

$info = @(
    'M8D EP3 event trace capture (firmware experiment 22)',
    "Time: $(Get-Date -Format o)",
    "Port: $Port",
    "Operator state label: $State",
    "Source build timestamp UTC: $($srcInfo.LastWriteTimeUtc.ToString('o'))",
    "ELF build timestamp UTC: $($elfInfo.LastWriteTimeUtc.ToString('o'))",
    "minichlink executable: $mini",
    "ELF SHA256: $elfHash",
    "Source SHA256: $srcHash",
    "Known M8D source SHA256 verification: PASS",
    "ELF diagnostic symbol verification: PASS",
    "ELF/source association: source hash and ELF symbol checked, but target-flashed image cannot be independently proven",
    'Read command: minichlink -r sram-20kb.bin ram 20480',
    'Warning: attaching the probe may halt or reset the MCU.'
)

try {
    Get-PnpDevice -PresentOnly -Class Ports -ErrorAction Stop |
        Format-List FriendlyName, Status, InstanceId |
        Out-String | Set-Content -LiteralPath (Join-Path $folder 'windows-ports.txt') -Encoding UTF8
}
catch {
    "Get-PnpDevice error: $($_.Exception.Message)" |
        Set-Content -LiteralPath (Join-Path $folder 'windows-ports.txt') -Encoding UTF8
}

Write-Host "Using: $mini"
Write-Host 'Reading 20 KiB SRAM with WCH-LinkE (probe may halt/reset MCU)...'
$exitCode = -1
$oldErrorAction = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
try {
    & $mini -r $ram ram 20480 2>&1 |
        Out-File -LiteralPath $log -Encoding UTF8
    $exitCode = $LASTEXITCODE
}
catch {
    "Exception invoking minichlink: $($_.Exception.Message)" |
        Add-Content -LiteralPath $log -Encoding UTF8
}
finally {
    $ErrorActionPreference = $oldErrorAction
}

$length = -1
if (Test-Path -LiteralPath $ram -PathType Leaf) {
    $length = (Get-Item -LiteralPath $ram).Length
}
$valid = ($exitCode -eq 0 -and $length -eq 20480)
$info += "minichlink exit code: $exitCode"
$elfHashAfter = (Get-FileHash -LiteralPath $elf -Algorithm SHA256).Hash.ToUpperInvariant()
$srcHashAfter = (Get-FileHash -LiteralPath $src -Algorithm SHA256).Hash.ToUpperInvariant()
$stableFiles = ($elfHashAfter -eq $elfHash -and $srcHashAfter -eq $srcHash)
$info += "ELF/source unchanged during capture: $stableFiles"
if (-not $stableFiles) { $valid = $false; $info += 'WARNING: source or ELF changed during capture; do not interpret dump' }
$info += "SRAM length: $length; valid: $valid"
$info += 'Trace ABI: magic=0x4D385452, version=1, record=36 bytes, rings=32+32; decode with Decode-M8D-Trace.py' 

$info | Set-Content -LiteralPath (Join-Path $folder 'capture-info.txt') -Encoding UTF8

Compress-Archive -Path (Join-Path $folder '*') -DestinationPath $zip -CompressionLevel Optimal -ErrorAction Stop
if ($valid) {
    Write-Host 'PASS: 20-KiB SRAM dump acquired with M8D ELF/source.' -ForegroundColor Green
}
else {
    Write-Warning "SRAM capture failed. Exit code=$exitCode, bytes=$length. See minichlink-read.log inside ZIP."
}
Write-Host "Diagnostic ZIP: $zip"
Write-Host 'No build, flash, explicit reset command, source/Git modification, or push performed. Probe attach may halt/reset MCU.'
