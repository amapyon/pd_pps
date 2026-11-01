# PD PPS - VS Code task helper for native Windows PowerShell 5.1.
# This file belongs at C:\pj\pd_pps\pd-pps.ps1 (outside the Git repository).
# No persistent PATH, registry, npm, driver, or repository configuration changes.

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Build', 'Flash', 'Probe', 'Log')]
    [string]$Action,

    [string]$Project = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$repoRoot = Join-Path $root 'src\pd_pps'
$gccBin = Join-Path $root 'tools\xpack-riscv-none-elf-gcc-14.2.0-3\bin'
$makeBin = Join-Path $root 'tools\xpack-windows-build-tools-4.4.1-3\bin'
$makeExe = Join-Path $makeBin 'make.exe'
$gccExe = Join-Path $gccBin 'riscv-none-elf-gcc.exe'
$minichlinkExe = Join-Path $repoRoot 'ch32fun\minichlink\minichlink.exe'

function Assert-File([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "File not found: $Path"
    }
}

function Get-ProjectDir([string]$RelativeProject) {
    if ([string]::IsNullOrWhiteSpace($RelativeProject)) {
        throw 'A project must be selected for Build or Flash.'
    }
    if ($RelativeProject -notmatch '^[A-Za-z0-9_./\\-]+$' -or
        $RelativeProject -match '(^|[/\\])\.\.($|[/\\])') {
        throw "Invalid project path: $RelativeProject"
    }
    if (-not (Test-Path -LiteralPath $repoRoot -PathType Container)) {
        throw "Repository not found: $repoRoot"
    }

    $base = [System.IO.Path]::GetFullPath($repoRoot).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    $fullPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $RelativeProject))
    if (-not $fullPath.StartsWith($base, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Project is outside the repository: $RelativeProject"
    }
    if (-not (Test-Path -LiteralPath $fullPath -PathType Container)) {
        throw "Project directory not found: $fullPath"
    }
    Assert-File (Join-Path $fullPath 'Makefile')
    return $fullPath
}

function Invoke-External([string]$Exe, [string[]]$Argv, [string]$Directory) {
    Assert-File $Exe
    Push-Location -LiteralPath $Directory
    try {
        & $Exe @Argv
        $code = $LASTEXITCODE
        if ($code -ne 0) {
            throw "Command failed (exit $code): $Exe $($Argv -join ' ')"
        }
    }
    finally {
        Pop-Location
    }
}

function Invoke-Build([string]$Directory) {
    Assert-File $gccExe
    Assert-File $makeExe
    $previousPath = $env:Path
    try {
        # Only this task process and its children see the toolchain PATH.
        $env:Path = "$gccBin;$makeBin;$previousPath"
        Write-Host "[PD PPS] Building: $Directory"
        Invoke-External $makeExe @('build', 'PREFIX=riscv-none-elf') $Directory
        Write-Host '[PD PPS] Build succeeded.'
    }
    finally {
        $env:Path = $previousPath
    }
}

function Get-FirmwareBin([string]$Directory) {
    $makeFile = Join-Path $Directory 'Makefile'
    $target = $null
    foreach ($line in (Get-Content -LiteralPath $makeFile)) {
        # Standard ch32fun Makefiles define e.g. TARGET := blink.
        if ($line -match '^\s*TARGET\s*(?::=|\?=|=)\s*([A-Za-z0-9_.-]+)\s*(?:#.*)?$') {
            $target = $Matches[1]
        }
    }
    if ($target) {
        $namedBin = Join-Path $Directory ($target + '.bin')
        Assert-File $namedBin
        return (Get-Item -LiteralPath $namedBin)
    }

    # Conservative fallback: never choose an *_ext.bin image.
    $candidates = @(Get-ChildItem -LiteralPath $Directory -File -Filter '*.bin' |
        Where-Object { $_.Name -notlike '*_ext.bin' })
    if ($candidates.Count -ne 1) {
        throw 'Cannot identify a single firmware .bin. Check TARGET in the Makefile; nothing was flashed.'
    }
    return $candidates[0]
}

switch ($Action) {
    'Build' {
        $dir = Get-ProjectDir $Project
        Invoke-Build $dir
    }
    'Flash' {
        $dir = Get-ProjectDir $Project
        # One action, one project: never prompt separately for build and flash.
        Invoke-Build $dir
        $firmware = Get-FirmwareBin $dir
        if ($firmware.Length -le 0) {
            throw "Firmware image is empty: $($firmware.FullName)"
        }
        Assert-File $minichlinkExe
        Write-Host "[PD PPS] Flashing: $($firmware.FullName) ($($firmware.Length) bytes)"
        Write-Host '[PD PPS] WARNING: This overwrites the connected MCU firmware.'
        Invoke-External $minichlinkExe @('-w', $firmware.Name, 'flash', '-b') $dir
        Write-Host '[PD PPS] Flash succeeded.'
    }
    'Probe' {
        Assert-File $minichlinkExe
        Write-Host '[PD PPS] Reading connected target information (no flash write).'
        Invoke-External $minichlinkExe @('-i') $repoRoot
    }
    'Log' {
        Assert-File $minichlinkExe
        Write-Host '[PD PPS] minichlink debugprintf terminal (-T); press Ctrl+C to stop.'
        Write-Host '[PD PPS] Requires firmware using ch32fun debugprintf; blink may output nothing.'
        Invoke-External $minichlinkExe @('-T') $repoRoot
    }
}
