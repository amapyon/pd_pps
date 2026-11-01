
# capture-workspace.ps1
# 開発用スクリプトとVS Code設定をGit管理領域へコピーする
# 実行中の開発環境には変更を加えない

[CmdletBinding()]
param(
    [switch]$IncludeEnv
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# スクリプト自身の配置場所からパスを決定
$destinationRoot = $PSScriptRoot

$repositoryRoot = (
    Resolve-Path (Join-Path $PSScriptRoot '..')
).Path

$workspaceRoot = (
    Resolve-Path (Join-Path $repositoryRoot '..\..')
).Path

# Git管理する開発環境ファイル
$files = @(
    'pd_pps.code-workspace'
    'organize-pdpps-workspace.ps1'
    'setup-workspace-view.ps1'
    'install-pd-pps-tools.ps1'
    'install-zadig.ps1'
    '.vscode\settings.json'
)

# env.ps1は環境依存情報があるため明示的に指定した場合のみ
if ($IncludeEnv) {
    $files += 'env.ps1'
}

Write-Host '===== Capture PD PPS Workspace ====='
Write-Host "Source: $workspaceRoot"
Write-Host "Target: $destinationRoot"
Write-Host ''

$copied = 0

foreach ($relativePath in $files) {

    $source = Join-Path $workspaceRoot $relativePath
    $destination = Join-Path $destinationRoot $relativePath

    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        Write-Host "[SKIP] $relativePath (not found)"
        continue
    }

    $parent = Split-Path -Parent $destination

    if (-not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Force -Path $parent |
            Out-Null
    }

    Copy-Item `
        -LiteralPath $source `
        -Destination $destination `
        -Force

    Write-Host "[COPY] $relativePath"
    $copied++
}

Write-Host ''
Write-Host "Captured files: $copied"
Write-Host ''

Write-Host '===== Git Status ====='
& git -C $repositoryRoot status --short

Write-Host ''
Write-Host 'Capture complete.'
Write-Host 'Review files before git add/commit.'
