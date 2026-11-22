# Shared, read-only tool resolution for Windows PowerShell 5.1. ASCII/LF.
function Get-DevEnvironmentConfig {
    param([Parameter(Mandatory=$true)][string]$ProjectRoot)
    $configFile = Join-Path $ProjectRoot 'config\dev_environment.json'
    if (-not (Test-Path -LiteralPath $configFile -PathType Leaf)) { throw "Environment config missing: $configFile" }
    $settings = Get-Content -LiteralPath $configFile -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($settings.schema_version -ne 1) { throw 'Unsupported environment schema' }
    if ([IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\') -ine [IO.Path]::GetFullPath($settings.root).TrimEnd('\')) {
        throw "Wrong project root: expected $($settings.root), actual $ProjectRoot"
    }
    return $settings
}
function Resolve-DevExecutable {
    param([Parameter(Mandatory=$true)][object]$ToolSpec,
          [Parameter(Mandatory=$true)][string]$ProjectRoot)
    if ($ToolSpec.PSObject.Properties.Name -contains 'relative_path') {
        $relative = [string]$ToolSpec.relative_path
        if ($relative -match '(^|[/\\])\.\.($|[/\\])' -or $relative -match '^[A-Za-z]:|^[/\\]') {
            throw "Unsafe configured relative path: $relative"
        }
        $candidate = Join-Path $ProjectRoot ($relative.Replace('/','\'))
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
        return $null
    }
    foreach ($candidateName in @($ToolSpec.commands)) {
        # -CommandType Application avoids function/alias shadowing (e.g. function Git).
        $found = @(Get-Command -Name ([string]$candidateName) -CommandType Application -ErrorAction SilentlyContinue)
        if ($found.Count -gt 0) { return [string]$found[0].Source }
    }
    return $null
}
function Get-DevTimestamp {
    return [DateTimeOffset]::Now.ToString('yyyy-MM-dd HH:mm:ss.fff zzz')
}
