# Common project tool resolver / process launcher. Uses safe argv via direct native invocation.
[CmdletBinding()]
param(
    [string]$ProjectRoot = 'C:\pj\pd_pps',
    [Parameter(Mandatory=$true)][ValidateSet('Git','Python','RiscvGcc','Make','NativeC','Minichlink')][string]$Tool,
    [string[]]$Arguments = @('--version'),
    [string]$WorkingDirectory = '',
    [switch]$AllowHardwareTool
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Dev-Environment.Common.ps1')
if ($PSVersionTable.PSVersion.Major -ne 5 -or $PSVersionTable.PSVersion.Minor -ne 1) { throw 'PowerShell 5.1 required' }
if ($Tool -eq 'Minichlink' -and -not $AllowHardwareTool) { throw 'Hardware tool blocked unless -AllowHardwareTool is explicit' }
$config = Get-DevEnvironmentConfig -ProjectRoot $ProjectRoot
$toolSpecs = @($config.tools | Where-Object { $_.id -ceq $Tool })
if ($toolSpecs.Count -ne 1) { throw "Unknown tool: $Tool" }
$executable = Resolve-DevExecutable -ToolSpec $toolSpecs[0] -ProjectRoot $ProjectRoot
if (-not $executable) { throw "Tool missing: $Tool (see Test-DevEnvironment.ps1)" }
if (-not $WorkingDirectory) { $WorkingDirectory = $ProjectRoot }
if (-not (Test-Path -LiteralPath $WorkingDirectory -PathType Container)) { throw "Working directory missing: $WorkingDirectory" }
Write-Host ("{0} | TOOL_START={1} exe={2}" -f (Get-DevTimestamp),$Tool,$executable)
Push-Location -LiteralPath $WorkingDirectory
try {
    # Native stderr does not reliably indicate failure; use process exit code.
    # Strict PS5 environments may promote native stderr to NativeCommandError.
    $oldEap = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $executable @Arguments
        $exitCode = $LASTEXITCODE
    } finally { $ErrorActionPreference = $oldEap }
    Write-Host ("{0} | TOOL_END={1} exit={2}" -f (Get-DevTimestamp),$Tool,$exitCode)
    if ($exitCode -ne 0) { throw "Tool failed: $Tool exit=$exitCode" }
} finally { Pop-Location }
