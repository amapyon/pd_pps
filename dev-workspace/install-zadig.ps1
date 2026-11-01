#Requires -Version 5.1
<#
.SYNOPSIS
    Download the portable Zadig 2.9 executable to the PD PPS tools directory.
.DESCRIPTION
    Downloads the official GitHub release, validates its Authenticode
    signature (Akeo Consulting), and installs only the executable file.
    Does NOT install or modify USB drivers, registry, PATH, or system settings.
.EXAMPLE
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\pj\pd_pps\install-zadig.ps1"
.EXAMPLE
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\pj\pd_pps\install-zadig.ps1" -Launch
#>
[CmdletBinding()]
param(
    [switch] $Launch
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$toolDir = 'C:\pj\pd_pps\tools'
$fileName = 'zadig-2.9.exe'
$destination = Join-Path $toolDir $fileName
$downloadUrl = 'https://github.com/pbatard/libwdi/releases/download/v1.5.1/zadig-2.9.exe'

function Assert-OfficialZadig {
    param([Parameter(Mandatory = $true)][string] $FilePath)

    $signature = Get-AuthenticodeSignature -LiteralPath $FilePath
    if ($signature.Status -ne 'Valid') {
        throw "Authenticode signature is not valid: $($signature.Status). File: $FilePath"
    }
    if ($null -eq $signature.SignerCertificate -or
        $signature.SignerCertificate.Subject -notmatch 'Akeo Consulting') {
        throw "Unexpected certificate signer. File: $FilePath"
    }

    Write-Host "Signature: VALID ($($signature.SignerCertificate.Subject))"
}

Write-Host 'PD PPS: Zadig 2.9 portable setup'
Write-Host "Destination: $destination"

if (Test-Path -LiteralPath $destination) {
    if ((Get-Item -LiteralPath $destination).PSIsContainer) {
        throw "A directory exists where the executable should be: $destination"
    }
    Write-Host 'The executable already exists. Checking its signature (no overwrite).'
    Assert-OfficialZadig -FilePath $destination
}
else {
    New-Item -ItemType Directory -Path $toolDir -Force | Out-Null

    # The staging directory is located under tools for a same-volume atomic move.
    $stagingDir = Join-Path $toolDir ('.zadig-download-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $stagingDir -ErrorAction Stop | Out-Null
    $tempFile = Join-Path $stagingDir $fileName

    try {
        Write-Host "Downloading: $downloadUrl"

        # Keep any TLS selection change local to this PowerShell process.
        $oldProtocol = [Net.ServicePointManager]::SecurityProtocol
        try {
            [Net.ServicePointManager]::SecurityProtocol = $oldProtocol -bor [Net.SecurityProtocolType]::Tls12
            Invoke-WebRequest -Uri $downloadUrl -OutFile $tempFile -UseBasicParsing -ErrorAction Stop
        }
        finally {
            [Net.ServicePointManager]::SecurityProtocol = $oldProtocol
        }

        Assert-OfficialZadig -FilePath $tempFile
        # File.Move never overwrites an existing destination.
        [System.IO.File]::Move($tempFile, $destination)
        Write-Host 'Zadig downloaded and verified.'
    }
    finally {
        if (Test-Path -LiteralPath $stagingDir) {
            Remove-Item -LiteralPath $stagingDir -Recurse -Force
        }
    }
}

$hash = Get-FileHash -LiteralPath $destination -Algorithm SHA256
Write-Host "SHA-256: $($hash.Hash)"
Write-Host "Ready: $destination"
Write-Host 'No USB driver has been changed.'

if ($Launch) {
    Write-Host 'Opening Zadig. Do not modify Interface 1 (COM port).'
    Start-Process -FilePath $destination
}
else {
    Write-Host 'To open Zadig, run:'
    Write-Host ('  & "' + $destination + '"')
}
