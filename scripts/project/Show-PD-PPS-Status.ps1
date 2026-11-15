[CmdletBinding()]
param([string]$Root = 'C:\pj\pd_pps')
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd('\')
Write-Host ('PD PPS project root: ' + $rootFull)
$files = @('README.md','ROADMAP.md','DEVELOPMENT_RULES.md','HANDOFF.md','pd_pps_zip_rules_v2.md', 'scripts\project\Initialize-Root-Git.ps1')
foreach ($rel in $files) {
    $exists = Test-Path -LiteralPath (Join-Path $rootFull $rel) -PathType Leaf
    Write-Host ('  {0,-10} {1}' -f $(if ($exists) {'FOUND'} else {'MISSING'}), $rel)
}
$git = Get-Command git -ErrorAction SilentlyContinue
if ($git -and (Test-Path -LiteralPath (Join-Path $rootFull '.git'))) {
    Write-Host '--- Git root ---'
    & git -C $rootFull rev-parse --show-toplevel
    Write-Host '--- Git status (no changes made) ---'
    & git -C $rootFull status --short
    Write-Host '--- Git remotes ---'
    & git -C $rootFull remote -v
} else { Write-Host 'Root Git repository not initialized or git.exe unavailable.' }
$logs = Join-Path $rootFull 'diagnostics\m43a_goodcrc\logs'
if (Test-Path -LiteralPath $logs -PathType Container) {
    $count = @(Get-ChildItem -LiteralPath $logs -Filter 'M43A_*.log' -File).Count
    Write-Host ('GoodCRC raw log count: ' + $count)
} else { Write-Host 'GoodCRC log folder not found (may not be installed yet).' }
Write-Host 'This command is read-only. It does NOT validate firmware or test status.'
