[CmdletBinding()]
param(
    [string]$ProjectRoot = 'C:\pj\pd_pps',
    [ValidateSet('Inspect','Build','HostC','Hardware')][string]$Profile = 'Inspect',
    [string]$ReportPath = '',
    [switch]$FailIfBlocked
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Dev-Environment.Common.ps1')
if ($PSVersionTable.PSVersion.Major -ne 5 -or $PSVersionTable.PSVersion.Minor -ne 1) {
    throw 'Windows PowerShell 5.1 required (not PowerShell 7)'
}
if (-not (Test-Path -LiteralPath (Join-Path $ProjectRoot '.git'))) { throw "Git root missing: $ProjectRoot" }
$config = Get-DevEnvironmentConfig -ProjectRoot $ProjectRoot
$results = New-Object System.Collections.ArrayList
$missingRequired = 0
$missingOptional = 0
foreach ($toolSpec in @($config.tools)) {
    $toolId = [string]$toolSpec.id
    $resolved = Resolve-DevExecutable -ToolSpec $toolSpec -ProjectRoot $ProjectRoot
    $isRequired = @($toolSpec.required_for) -contains $Profile
    if (-not $resolved) {
        if ($isRequired) { $missingRequired += 1; $status = 'BLOCKED' }
        else { $missingOptional += 1; $status = 'OPTIONAL_MISSING' }
    } else { $status = 'FOUND' }
    $result = [ordered]@{ tool=$toolId; status=$status; required=$isRequired; path=$resolved }
    [void]$results.Add($result)
    Write-Host ("{0} | {1}: {2} {3}" -f (Get-DevTimestamp),$toolId,$status,$resolved)
}
$finalStatus = if ($missingRequired -gt 0) { 'BLOCKED' } elseif ($missingOptional -gt 0) { 'OPTIONAL_MISSING' } else { 'READY' }
$summary = [ordered]@{
    schema_version=1; project_root=$ProjectRoot; profile=$Profile;
    shell=$PSVersionTable.PSVersion.ToString();
    timestamp=[DateTimeOffset]::Now.ToString('o');
    result=$finalStatus; missing_required=$missingRequired; missing_optional=$missingOptional;
    tools=@($results.ToArray()); note='OPTIONAL_MISSING is not a passed test. No hardware I/O.'
}
Write-Host ("{0} | ENVIRONMENT_RESULT={1} profile={2} required_missing={3} optional_missing={4}" -f (Get-DevTimestamp),$finalStatus,$Profile,$missingRequired,$missingOptional)
if ($ReportPath) {
    $parent = Split-Path -Parent $ReportPath
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) { throw "Report directory missing: $parent" }
    $summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
    if (-not (Test-Path -LiteralPath $ReportPath -PathType Leaf)) { throw 'Environment report not created' }
}
if ($FailIfBlocked -and $finalStatus -eq 'BLOCKED') { throw "Environment profile blocked: $Profile" }
