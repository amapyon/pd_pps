
# setup-workspace-view.ps1
# Windows PowerShell 5.1
# Inspect: no changes
# Apply: move README into Git repository and expose workspace tools

[CmdletBinding()]
param(
    [ValidateSet('Inspect', 'Apply')]
    [string]$Mode = 'Inspect'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = 'C:\pj\pd_pps'
$repo = Join-Path $root 'src\pd_pps'
$workspacePath = Join-Path $root 'pd_pps.code-workspace'
$repoReadme = Join-Path $repo 'README.md'
$settingsPath = Join-Path $root '.vscode\settings.json'

$readmeCandidates = @(
    (Join-Path $root 'README.md'),
    (Join-Path $root 'README_PD_PPS_Workspace.md')
)

$sourceReadme = @(
    $readmeCandidates |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
) | Select-Object -First 1

if (-not (Test-Path -LiteralPath $workspacePath)) {
    throw "Workspace not found: $workspacePath"
}
if (-not (Test-Path -LiteralPath $repo -PathType Container)) {
    throw "Repository not found: $repo"
}

if ($sourceReadme -and (Test-Path -LiteralPath $repoReadme)) {
    throw 'README exists in both locations. Merge manually before applying.'
}

if (-not $sourceReadme -and
    -not (Test-Path -LiteralPath $repoReadme)) {
    throw 'README not found. Place README.md in the workspace root first.'
}

# Read current workspace.
$raw = [System.IO.File]::ReadAllText($workspacePath)
$config = $raw | ConvertFrom-Json

$repoFolders = @(
    $config.folders | Where-Object {
        $_.name -eq 'pd_pps' -and
        ($_.path -replace '\\', '/') -eq 'src/pd_pps'
    }
)

if ($repoFolders.Count -ne 1) {
    throw 'Expected pd_pps source folder not found in workspace.'
}

$toolsFolders = @(
    $config.folders | Where-Object {
        $_.path -eq '.'
    }
)

Write-Host ''
Write-Host '===== WORKSPACE VIEW SETUP ====='
Write-Host "Mode: $Mode"
Write-Host "Workspace: $workspacePath"
Write-Host "Git repository: $repo"
Write-Host "README source: $sourceReadme"
Write-Host "README target: $repoReadme"
Write-Host "Workspace Tools registered: $($toolsFolders.Count -gt 0)"
Write-Host "Folder settings exists: $(Test-Path $settingsPath)"
Write-Host ''

if ($Mode -eq 'Inspect') {
    Write-Host 'No changes made.'
    return
}

# Back up the workspace and the README.
$backupDir = Join-Path $root 'workspace-backups'
New-Item -ItemType Directory -Force -Path $backupDir |
    Out-Null

$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'

Copy-Item -LiteralPath $workspacePath `
    -Destination (Join-Path $backupDir "$timestamp.code-workspace")

if ($sourceReadme) {
    Copy-Item -LiteralPath $sourceReadme `
        -Destination (Join-Path $backupDir "$timestamp.README.md")
}

# Add outer development folder to VS Code.
if ($toolsFolders.Count -eq 0) {
    $toolsFolder = [pscustomobject]@{
        name = 'Workspace Tools'
        path = '.'
    }
    $config.folders = @($config.folders) + @($toolsFolder)
}

# Previously, workspaceFolder meant the source repository.
# Explicitly qualify it before switching to multi-root mode.
$json = $config | ConvertTo-Json -Depth 100
$json = $json.Replace(
    '${workspaceFolder}',
    '${workspaceFolder:pd_pps}'
)
$json = $json.Replace(
    '${workspaceFolderBasename}',
    '${workspaceFolderBasename:pd_pps}'
)

# Validate generated JSON before saving.
$null = $json | ConvertFrom-Json

$utf8 = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText(
    $workspacePath,
    $json + [Environment]::NewLine,
    $utf8
)

# Folder-specific Explorer settings for Workspace Tools.
# Existing settings.json is never overwritten.
if (-not (Test-Path -LiteralPath $settingsPath)) {
    $settingsDir = Split-Path -Parent $settingsPath
    New-Item -ItemType Directory -Force -Path $settingsDir |
        Out-Null

    $folderSettings = @{
        'files.exclude' = @{
            'src' = $true
            'tools' = $true
            'workspace-backups' = $true
        }
        'search.exclude' = @{
            'src' = $true
            'tools' = $true
            'workspace-backups' = $true
        }
    }

    [System.IO.File]::WriteAllText(
        $settingsPath,
        ($folderSettings | ConvertTo-Json -Depth 20),
        $utf8
    )
}
else {
    Write-Warning 'Existing folder settings were preserved.'
    Write-Warning 'If src/tools appear twice, adjust .vscode/settings.json manually.'
}

# Move the README into the Git repository.
if ($sourceReadme) {
    Move-Item -LiteralPath $sourceReadme `
        -Destination $repoReadme
}

Write-Host ''
Write-Host '===== COMPLETED ====='
Write-Host "README: $repoReadme"
Write-Host "Workspace: $workspacePath"
Write-Host 'Git commit was NOT performed.'
Write-Host 'Reopen the workspace in VS Code.'
