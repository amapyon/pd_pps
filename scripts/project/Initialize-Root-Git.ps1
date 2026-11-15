[CmdletBinding()]
param(
    [switch]$Apply,
    [string]$Root = 'C:\pj\pd_pps'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd('\')
if (-not (Test-Path -LiteralPath $rootFull -PathType Container)) { throw ('Missing root: ' + $rootFull) }
$source = Join-Path $rootFull 'src\pd_pps'
if (-not (Test-Path -LiteralPath $source -PathType Container)) { throw ('Missing source directory: ' + $source) }
$git = Get-Command git -ErrorAction SilentlyContinue
if (-not $git) { throw 'git.exe not found. Install Git for Windows first.' }
$rootMeta = Join-Path $rootFull '.git'
$srcMeta = Join-Path $source '.git'
$submoduleSource = Join-Path $source 'ch32fun'
$subMeta = Join-Path $submoduleSource '.git'
$remove = @()
if (Test-Path -LiteralPath $subMeta) { $remove += $subMeta }
if (Test-Path -LiteralPath $srcMeta) { $remove += $srcMeta }
if (Test-Path -LiteralPath $rootMeta) {
    $reported = (& git -C $rootFull rev-parse --show-toplevel 2>$null)
    if ($LASTEXITCODE -ne 0 -or -not $reported) { throw ('Root .git exists but is not a valid repository: ' + $rootMeta) }
    $reportedPath = [System.IO.Path]::GetFullPath(([string]$reported).Trim()).TrimEnd('\')
    if (-not $reportedPath.Equals($rootFull,[System.StringComparison]::OrdinalIgnoreCase)) {
        throw ('A different Git root already applies: ' + $reportedPath)
    }
    $needsInit = $false
} else { $needsInit = $true }
Write-Host ('Proposed single Git root: ' + $rootFull)
Write-Host ('Init new root repository: ' + $needsInit)
foreach ($m in $remove) { Write-Host ('Remove Git metadata ONLY: ' + $m) }
Write-Host 'No source file/log removal, no backups, no git add/commit/remote/push.'
Write-Host 'NOTICE: removed Git history/metadata CANNOT be restored by this script.'
if (-not $Apply) { Write-Host 'DRY RUN: no changes. Re-run with -Apply after reviewing the plan.'; exit 0 }
if ((Test-Path -LiteralPath $subMeta) -and (Test-Path -LiteralPath $submoduleSource -PathType Container)) {
    $sourceFiles = @(Get-ChildItem -LiteralPath $submoduleSource -Force | Where-Object { $_.Name -ne '.git' })
    if ($sourceFiles.Count -eq 0) { throw 'ch32fun has no checked-out source files. Migration aborted before deletion.' }
}
foreach ($m in $remove) {
    if ((Test-Path -LiteralPath $m -PathType Container)) { Remove-Item -LiteralPath $m -Force -Recurse }
    elseif (Test-Path -LiteralPath $m -PathType Leaf) { Remove-Item -LiteralPath $m -Force }
}
if ($needsInit) {
    & git init -b main $rootFull
    if ($LASTEXITCODE -ne 0) { throw 'git init failed; source files were not deleted.' }
}
$top = (& git -C $rootFull rev-parse --show-toplevel 2>$null)
if ($LASTEXITCODE -ne 0) { throw 'Root Git validation failed.' }
Write-Host ('Root Git: ' + ([string]$top).Trim())
Write-Host 'NEXT: Review git status and .gitignore; stage only the intended files.'
Write-Host 'Git remote and GitHub histories are NOT configured or updated.'
