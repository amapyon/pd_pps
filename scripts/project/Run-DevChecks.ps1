# One command for future chats: static checks + environment profiles + one report.
[CmdletBinding()]
param([string]$ProjectRoot = 'C:\pj\pd_pps')
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSVersion.Major -ne 5 -or $PSVersionTable.PSVersion.Minor -ne 1) { throw 'Windows PowerShell 5.1 required' }
$runId = (Get-Date).ToString('yyyyMMdd_HHmmss') + '_' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$runDirectory = Join-Path $ProjectRoot ('diagnostics\environment\runs\'+$runId)
New-Item -ItemType Directory -Path $runDirectory -Force -ErrorAction Stop | Out-Null
$runLog = Join-Path $runDirectory 'runner.log'
function Write-DevLog([string]$Line) {
    $timestamp = [DateTimeOffset]::Now.ToString('yyyy-MM-dd HH:mm:ss.fff zzz')
    $textLine = "$timestamp | $Line"
    Write-Host $textLine
    Add-Content -LiteralPath $runLog -Value $textLine -Encoding UTF8
}
try {
    Write-DevLog 'DEV_CHECKS_START (no Flash, no Git mutations, no new dependencies)'
    & (Join-Path $PSScriptRoot 'Test-PowerShellSafety.ps1') -ProjectRoot $ProjectRoot
    foreach ($profileName in @('Inspect','Build','HostC','Hardware')) {
        $reportFile = Join-Path $runDirectory ("environment_$($profileName).json")
        & (Join-Path $PSScriptRoot 'Test-DevEnvironment.ps1') -ProjectRoot $ProjectRoot -Profile $profileName -ReportPath $reportFile
        if (-not (Test-Path -LiteralPath $reportFile -PathType Leaf)) { throw "Report missing: $reportFile" }
    }
    Write-DevLog 'DEV_CHECKS_COMPLETE (profiles reported independently; OPTIONAL_MISSING is not PASS)'
    Write-DevLog "REPORT_DIR=$runDirectory"
} catch {
    Write-DevLog "DEV_CHECKS_STOPPED: $($_.Exception.Message)"
    throw
}
