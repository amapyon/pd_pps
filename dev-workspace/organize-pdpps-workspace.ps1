
# organize-pdpps-workspace.ps1
# Windows PowerShell 5.1 / Windows 11
# VS Code workspace maintenance for PD PPS experiments.
#
# Inspect: Read-only inventory.
# Apply:   Backup and update the VS Code workspace file.
#
# Never moves/deletes source files, builds firmware, or flashes the MCU.

[CmdletBinding()]
param(
    [ValidateSet('Inspect', 'Apply')]
    [string]$Mode = 'Inspect',

    [string]$Root = 'C:\pj\pd_pps',

    [string]$WorkspaceFile = '',

    [string]$DefaultProject = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Set-JsonProperty {
    param(
        [object]$Object,
        [string]$Name,
        [object]$Value
    )

    $p = $Object.PSObject.Properties[$Name]

    if ($null -ne $p) {
        $p.Value = $Value
    }
    else {
        $Object | Add-Member `
            -MemberType NoteProperty `
            -Name $Name `
            -Value $Value
    }
}

function Add-ExcludePatterns {
    param(
        [object]$Settings,
        [string]$SettingName,
        [string[]]$Patterns
    )

    $p = $Settings.PSObject.Properties[$SettingName]

    if ($null -eq $p -or $null -eq $p.Value) {
        $rules = [pscustomobject]@{}
        Set-JsonProperty $Settings $SettingName $rules
    }
    else {
        $rules = $p.Value
    }

    foreach ($pattern in $Patterns) {
        # Preserve existing user preferences, including explicit false.
        if ($null -eq $rules.PSObject.Properties[$pattern]) {
            $rules | Add-Member `
                -MemberType NoteProperty `
                -Name $pattern `
                -Value $true
        }
    }
}

# --------------------------------------------------
# 1. Validate layout
# --------------------------------------------------

$Root = [System.IO.Path]::GetFullPath($Root)
$repo = Join-Path $Root 'src\pd_pps'
$helper = Join-Path $Root 'pd-pps.ps1'

if ([string]::IsNullOrWhiteSpace($WorkspaceFile)) {
    $WorkspaceFile = Join-Path $Root 'pd_pps.code-workspace'
}

$WorkspaceFile = [System.IO.Path]::GetFullPath(
    $WorkspaceFile
)

if (-not (Test-Path -LiteralPath $repo -PathType Container)) {
    throw "Repository directory not found: $repo"
}

if (-not (Test-Path -LiteralPath $WorkspaceFile -PathType Leaf)) {
    throw "Workspace file not found: $WorkspaceFile"
}

# --------------------------------------------------
# 2. Discover experiment projects
#    Only top-level NN_* directories are included.
#    Nested experiments are supported.
#    ch32fun examples are not included.
# --------------------------------------------------

$repoPrefix = $repo.TrimEnd('\', '/') + '\'

$projects = @(
    foreach ($dir in (
        Get-ChildItem -LiteralPath $repo -Directory |
        Where-Object { $_.Name -match '^[0-9]{2}_' }
    )) {
        Get-ChildItem `
            -LiteralPath $dir.FullName `
            -Filter 'Makefile' `
            -File `
            -Recurse |
        ForEach-Object {
            $relative = $_.DirectoryName.Substring(
                $repoPrefix.Length
            )
            $relative.Replace('\', '/')
        }
    }
) | Sort-Object -Unique

$projects = @($projects)

if ($projects.Count -eq 0) {
    throw "No experiment Makefiles found in: $repo"
}

# --------------------------------------------------
# 3. Read current VS Code workspace
# --------------------------------------------------

try {
    $original = [System.IO.File]::ReadAllText(
        $WorkspaceFile,
        [System.Text.Encoding]::UTF8
    )

    $config = $original | ConvertFrom-Json
}
catch {
    throw "Cannot parse workspace JSON. No changes made. $($_.Exception.Message)"
}

$matchingFolders = @(
    $config.folders | Where-Object {
        $_.name -eq 'pd_pps' -and
        ($_.path -replace '\\', '/') -eq 'src/pd_pps'
    }
)

if ($matchingFolders.Count -ne 1) {
    throw 'Workspace folder layout differs from expected src/pd_pps. No changes made.'
}

if ($null -eq $config.tasks) {
    throw 'Workspace tasks not found. No changes made.'
}

$projectInputs = @(
    $config.tasks.inputs |
    Where-Object { $_.id -eq 'pdppsProject' }
)

if ($projectInputs.Count -ne 1) {
    throw 'Expected one pdppsProject input. No changes made.'
}

$projectInput = $projectInputs[0]

$previousOptions = @($projectInput.options)

$missing = @(
    $projects |
    Where-Object { $_ -notin $previousOptions }
)

$obsolete = @(
    $previousOptions |
    Where-Object { $_ -notin $projects }
)

# --------------------------------------------------
# 4. Inventory report
# --------------------------------------------------

Write-Host ''
Write-Host '===== PD PPS WORKSPACE INVENTORY ====='
Write-Host "Mode:       $Mode"
Write-Host "Root:       $Root"
Write-Host "Repository: $repo"
Write-Host "Workspace:  $WorkspaceFile"
Write-Host "Helper:     $helper"
Write-Host ''

Write-Host "Detected projects: $($projects.Count)"

foreach ($project in $projects) {
    Write-Host "  $project"
}

Write-Host ''
Write-Host "Workspace project options: $($previousOptions.Count)"

Write-Host ''
Write-Host "Missing from workspace: $($missing.Count)"
foreach ($item in $missing) {
    Write-Host "  + $item"
}

Write-Host ''
Write-Host "Obsolete workspace entries: $($obsolete.Count)"
foreach ($item in $obsolete) {
    Write-Host "  - $item"
}

Write-Host ''
Write-Host 'Registered VS Code tasks:'
foreach ($task in @($config.tasks.tasks)) {
    Write-Host "  $($task.label)"
}

Write-Host ''
Write-Host "Build helper exists: $(Test-Path -LiteralPath $helper)"

# Git status is informational only.
$git = Get-Command git -ErrorAction SilentlyContinue
if ($null -ne $git -and
    (Test-Path -LiteralPath (Join-Path $repo '.git'))) {

    Write-Host ''
    Write-Host 'Git status (short):'
    & git -C $repo status --short
}

if ($Mode -eq 'Inspect') {
    Write-Host ''
    Write-Host 'INSPECT COMPLETE: no files changed.'
    Write-Host 'Run with -Mode Apply after reviewing this report.'
    return
}

# --------------------------------------------------
# 5. Apply: verify helper and project choice
# --------------------------------------------------

if (-not (Test-Path -LiteralPath $helper -PathType Leaf)) {
    throw "Build helper not found: $helper"
}

if ([string]::IsNullOrWhiteSpace($DefaultProject)) {
    # Choose the newest numbered experiment as initial selection.
    $DefaultProject = $projects[-1]
}

if ($DefaultProject -notin $projects) {
    throw "Default project is not a detected Makefile project: $DefaultProject"
}

# --------------------------------------------------
# 6. Update project selection list
# --------------------------------------------------

Set-JsonProperty $projectInput 'options' $projects
Set-JsonProperty $projectInput 'default' $DefaultProject

# --------------------------------------------------
# 7. Improve workspace visibility
# --------------------------------------------------

if ($null -eq $config.settings) {
    Set-JsonProperty `
        $config `
        'settings' `
        ([pscustomobject]@{})
}

$buildPatterns = @(
    '**/*.bin'
    '**/*.elf'
    '**/*.hex'
    '**/*.map'
    '**/*.lst'
    '**/*.o'
    '**/*.d'
)

# Hide only generated build files in Explorer.
Add-ExcludePatterns `
    $config.settings `
    'files.exclude' `
    $buildPatterns

# Reduce noise in global source searches.
$searchPatterns = @(
    $buildPatterns
    '**/ch32fun/**'
)

Add-ExcludePatterns `
    $config.settings `
    'search.exclude' `
    $searchPatterns

# Preserve existing tasks, commands, folder paths,
# and other workspace settings.

# --------------------------------------------------
# 8. Backup and save
# --------------------------------------------------

$newJson = $config | ConvertTo-Json -Depth 100

if ($original.Trim() -ceq $newJson.Trim()) {
    Write-Host ''
    Write-Host 'No workspace changes are necessary.'
    return
}

$backupDir = Join-Path $Root 'workspace-backups'

if (-not (Test-Path -LiteralPath $backupDir)) {
    New-Item `
        -ItemType Directory `
        -Path $backupDir |
        Out-Null
}

$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$backupName = "$timestamp-$([System.IO.Path]::GetFileName($WorkspaceFile))"
$backupFile = Join-Path $backupDir $backupName

Copy-Item `
    -LiteralPath $WorkspaceFile `
    -Destination $backupFile `
    -ErrorAction Stop

$utf8 = New-Object System.Text.UTF8Encoding($false)

[System.IO.File]::WriteAllText(
    $WorkspaceFile,
    $newJson + [Environment]::NewLine,
    $utf8
)

Write-Host ''
Write-Host '===== WORKSPACE UPDATED ====='
Write-Host "Backup:          $backupFile"
Write-Host "Workspace:       $WorkspaceFile"
Write-Host "Projects:        $($projects.Count)"
Write-Host "Default project: $DefaultProject"
Write-Host ''
Write-Host 'No source files or Makefiles were modified.'
Write-Host 'Reopen the workspace in VS Code.'
