# PD PPS: portable xPack toolchain installer for Windows 11 x64.
# Installs only beneath C:\pj\pd_pps\tools.
# No admin privileges, npm, xpm, persistent PATH, or driver changes.
# PowerShell 5.1 and 7 compatible.

param(
    [string]$Root = 'C:\pj\pd_pps'
)

$ErrorActionPreference = 'Stop'

if (-not [Environment]::Is64BitOperatingSystem) {
    throw 'This installer requires 64-bit Windows.'
}

$toolsDir = Join-Path $Root 'tools'
if ((Test-Path -LiteralPath $toolsDir) -and
    -not (Test-Path -LiteralPath $toolsDir -PathType Container)) {
    throw "Not a directory: $toolsDir"
}
New-Item -ItemType Directory -Path $toolsDir -Force | Out-Null

$packages = @(
    @{
        Name = 'xpack-riscv-none-elf-gcc-14.2.0-3'
        Repository = 'riscv-none-elf-gcc-xpack'
        Tag = 'v14.2.0-3'
        Archive = 'xpack-riscv-none-elf-gcc-14.2.0-3-win32-x64.zip'
        SHA256 = '9bb15efdeca256532c4a83ce6462c7dc1f9cfebe1f1f43d581b2ad7d077209b6'
        Executable = 'bin\riscv-none-elf-gcc.exe'
        VersionRegex = '14\.2\.0'
    },
    @{
        Name = 'xpack-windows-build-tools-4.4.1-3'
        Repository = 'windows-build-tools-xpack'
        Tag = 'v4.4.1-3'
        Archive = 'xpack-windows-build-tools-4.4.1-3-win32-x64.zip'
        SHA256 = '113d4dfdbbc56dc9b865c9f75d38cd0da82f0d7094187e6f7a803fe6eef1d218'
        Executable = 'bin\make.exe'
        VersionRegex = 'GNU Make 4\.4\.1'
    }
)

function Assert-ToolVersion {
    param(
        [string]$ExePath,
        [string]$Pattern
    )

    if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
        throw "Executable not found: $ExePath"
    }

    $versionText = (& $ExePath --version 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) {
        throw "Version check failed ($LASTEXITCODE): $ExePath`n$versionText"
    }
    if ($versionText -notmatch $Pattern) {
        throw "Unexpected version: $ExePath`n$versionText"
    }
    Write-Host (($versionText.Trim() -split "`r?`n")[0])
}

foreach ($pkg in $packages) {
    $destination = Join-Path $toolsDir $pkg.Name
    $installedExe = Join-Path $destination $pkg.Executable

    if (Test-Path -LiteralPath $destination) {
        Write-Host "[EXISTS] $destination (never overwritten)"
        Assert-ToolVersion -ExePath $installedExe -Pattern $pkg.VersionRegex
        continue
    }

    # Unique staging directory within the project: safe to remove after this run.
    $stage = Join-Path $toolsDir ('.stage-' + $pkg.Name + '-' + [Guid]::NewGuid().ToString('N'))
    $archivePath = Join-Path $stage $pkg.Archive
    $extractPath = Join-Path $stage 'extracted'
    $url = 'https://github.com/xpack-dev-tools/' + $pkg.Repository +
           '/releases/download/' + $pkg.Tag + '/' + $pkg.Archive

    New-Item -ItemType Directory -Path $stage | Out-Null
    try {
        Write-Host "[DOWNLOAD] $($pkg.Archive)"
        Invoke-WebRequest -Uri $url -OutFile $archivePath

        $actualHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
        if ($actualHash -ne $pkg.SHA256) {
            throw "SHA-256 mismatch for $($pkg.Archive). Expected $($pkg.SHA256); got $actualHash"
        }
        Write-Host '[SHA-256] OK'

        Write-Host "[EXTRACT] $($pkg.Name)"
        Expand-Archive -LiteralPath $archivePath -DestinationPath $extractPath

        # xPack ZIP files may contain files directly or one top-level directory.
        $candidates = @($extractPath)
        $candidates += @(Get-ChildItem -LiteralPath $extractPath -Directory |
            Select-Object -ExpandProperty FullName)
        $validCandidates = @($candidates | Where-Object {
            Test-Path -LiteralPath (Join-Path $_ $pkg.Executable) -PathType Leaf
        })
        if ($validCandidates.Count -ne 1) {
            throw "Expected exactly one $($pkg.Executable) inside ZIP; found $($validCandidates.Count)."
        }
        $sourceDir = $validCandidates[0]

        # Verify before publishing to the final location.
        Assert-ToolVersion -ExePath (Join-Path $sourceDir $pkg.Executable) -Pattern $pkg.VersionRegex
        if (Test-Path -LiteralPath $destination) {
            throw "Destination appeared during installation, refusing overwrite: $destination"
        }
        Move-Item -LiteralPath $sourceDir -Destination $destination
        Write-Host "[INSTALLED] $destination"
    }
    finally {
        if (Test-Path -LiteralPath $stage) {
            Remove-Item -LiteralPath $stage -Recurse -Force
        }
    }
}

Write-Host ''
Write-Host 'All tools are ready. Existing PATH, registry, npm and drivers were not changed.'
Write-Host "GCC:  $(Join-Path $toolsDir 'xpack-riscv-none-elf-gcc-14.2.0-3\bin\riscv-none-elf-gcc.exe')"
Write-Host "Make: $(Join-Path $toolsDir 'xpack-windows-build-tools-4.4.1-3\bin\make.exe')"
