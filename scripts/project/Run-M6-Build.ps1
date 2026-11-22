# M6 BuildOnly integration: uses the canonical project tool environment. NO MCU I/O.
[CmdletBinding()]
param([string]$ProjectRoot='C:\pj\pd_pps')
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if ($PSVersionTable.PSVersion.Major -ne 5 -or $PSVersionTable.PSVersion.Minor -ne 1) {
    throw 'Windows PowerShell 5.1 required'
}
. (Join-Path $PSScriptRoot 'Dev-Environment.Common.ps1')
$script:runDirectory=$null
$script:runLog=$null
$script:hostResult='NOT_TESTED'
$script:buildResult='NOT_TESTED'
function Write-M6Log([string]$Line) {
    $row=('{0} | {1}' -f (Get-DevTimestamp),$Line)
    Write-Host $row
    if ($script:runLog) { Add-Content -LiteralPath $script:runLog -Value $row -Encoding UTF8 }
}
function Invoke-M6Process([string]$Executable,[string[]]$Argv,[string]$Cwd,[string]$Tag) {
    $stdout=Join-Path $script:runDirectory ($Tag+'_stdout.raw.txt')
    $stderr=Join-Path $script:runDirectory ($Tag+'_stderr.raw.txt')
    Write-M6Log "PROCESS_START tag=$Tag exe=$Executable"
    $proc=Start-Process -FilePath $Executable -ArgumentList $Argv -WorkingDirectory $Cwd `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr `
        -Wait -PassThru -ErrorAction Stop
    Write-M6Log "PROCESS_END tag=$Tag exit=$($proc.ExitCode) stdout=$stdout stderr=$stderr"
    if ($proc.ExitCode -ne 0) {
        # Show failure evidence in this same invocation. Preserve the raw files byte-for-byte.
        foreach ($source in @($stderr,$stdout)) {
            if (Test-Path -LiteralPath $source -PathType Leaf) {
                Write-M6Log "ERROR_LOG_TAIL=$source"
                Get-Content -LiteralPath $source -Tail 80 | ForEach-Object { Write-Host $_ }
            }
        }
        throw "Process failed: $Tag exit=$($proc.ExitCode)"
    }
    return $proc.ExitCode
}
try {
    if (-not (Test-Path -LiteralPath (Join-Path $ProjectRoot '.git'))) {
        throw "Git root missing: $ProjectRoot"
    }
    $config=Get-DevEnvironmentConfig -ProjectRoot $ProjectRoot
    $runId=(Get-Date).ToString('yyyyMMdd_HHmmss')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
    $script:runDirectory=Join-Path $ProjectRoot ('diagnostics\m6_usb_update\runs\'+$runId)
    New-Item -Path $script:runDirectory -ItemType Directory -Force -ErrorAction Stop | Out-Null
    $script:runLog=Join-Path $script:runDirectory 'runner.log'
    Write-M6Log 'M6_BUILD_ONLY_START: no source edits, MCU probe, reset, USB ISP transition, or Flash'
    $target=Join-Path $ProjectRoot 'src\pd_pps\firmware\experiments\m6_usb_isp_entry'
    $manifestKit=Join-Path $ProjectRoot 'fw_m6_usb_isp_entry_v03_20261122'
    $installer=Join-Path $manifestKit 'install.ps1'
    if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
        throw "M6 v03 installed source verifier missing: $installer"
    }
    $psCommand=Get-Command -Name 'powershell.exe' -CommandType Application -ErrorAction Stop
    $psExe=[string]$psCommand.Source
    Invoke-M6Process $psExe @('-NoProfile','-ExecutionPolicy','Bypass','-File',$installer,
        '-ProjectRoot',$ProjectRoot) $ProjectRoot 'verify_m6_v03' | Out-Null
    $makefile=Join-Path $target 'Makefile'
    if (-not (Test-Path -LiteralPath $makefile -PathType Leaf)) { throw 'M6 Makefile missing' }
    $makeText=Get-Content -LiteralPath $makefile -Raw -Encoding UTF8
    if ($makeText -notmatch '(?m)^EXTRA_CFLAGS\s*\+=.*-DM6_BOOT_ENTRY_ENABLED=0\b') {
        throw 'BOOT transition is not proven disabled: refuse build'
    }
    Write-M6Log 'M6_SOURCE_VERIFIED; BOOT_ENTRY_DISABLED'
    $report=Join-Path $script:runDirectory 'environment_Build.json'
    & (Join-Path $PSScriptRoot 'Test-DevEnvironment.ps1') `
        -ProjectRoot $ProjectRoot -Profile Build -ReportPath $report -FailIfBlocked
    if (-not (Test-Path -LiteralPath $report -PathType Leaf)) { throw 'Build profile report missing' }
    $envResult=Get-Content -LiteralPath $report -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($envResult.missing_required -ne 0) { throw 'Build environment prerequisites missing' }
    $nativeSpec=@($config.tools | Where-Object { $_.id -ceq 'NativeC' })
    $pythonSpec=@($config.tools | Where-Object { $_.id -ceq 'Python' })
    if ($nativeSpec.Count -ne 1 -or $pythonSpec.Count -ne 1) { throw 'Tool definitions missing' }
    $native=Resolve-DevExecutable -ToolSpec $nativeSpec[0] -ProjectRoot $ProjectRoot
    $python=Resolve-DevExecutable -ToolSpec $pythonSpec[0] -ProjectRoot $ProjectRoot
    if (-not $native -or -not $python) {
        Write-M6Log 'HOST_TESTS_NOT_TESTED: Windows native C compiler or Python unavailable; NOT PASS'
    } else {
        $hostTestScript=Join-Path $target 'tests\test_m6_usb_entry.py'
        if (-not (Test-Path -LiteralPath $hostTestScript -PathType Leaf)) {
            throw 'Expected M6 host test script missing'
        }
        $priorCC=$env:CC
        try {
            $env:CC=$native
            Invoke-M6Process $python @($hostTestScript) $target 'host_c_tests' | Out-Null
        } finally { $env:CC=$priorCC }
        $script:hostResult='PASS'
        Write-M6Log 'HOST_TESTS_PASS: isolated C state machine only'
    }
    $buildHelper=Join-Path $ProjectRoot ([string]$config.paths.build_helper).Replace('/','\')
    if (-not (Test-Path -LiteralPath $buildHelper -PathType Leaf)) {
        throw "Project build helper missing: $buildHelper"
    }
    Invoke-M6Process $psExe @('-NoProfile','-ExecutionPolicy','Bypass','-File',$buildHelper,
        '-Action','Build','-Project','firmware/experiments/m6_usb_isp_entry') $ProjectRoot 'cross_build' | Out-Null
    $artifacts=New-Object System.Collections.ArrayList
    foreach ($filename in @('m6_usb_isp_entry.bin','m6_usb_isp_entry.elf')) {
        $artifact=Join-Path $target $filename
        if (-not (Test-Path -LiteralPath $artifact -PathType Leaf)) { throw "Build output missing: $artifact" }
        $length=(Get-Item -LiteralPath $artifact).Length
        if ($length -le 0) { throw "Empty build output: $artifact" }
        if ($filename.EndsWith('.bin') -and $length -gt 63488) { throw 'Image exceeds 62 KiB code Flash' }
        $digest=(Get-FileHash -LiteralPath $artifact -Algorithm SHA256).Hash
        [void]$artifacts.Add([ordered]@{name=$filename;bytes=$length;sha256=$digest;path=$artifact})
        Write-M6Log "BUILD_ARTIFACT name=$filename bytes=$length sha256=$digest"
    }
    $script:buildResult='PASS'
    $result=[ordered]@{
        schema_version=1; timestamp=[DateTimeOffset]::Now.ToString('o');
        root=$ProjectRoot; result='BUILD_ONLY_COMPLETE'; host_c=$script:hostResult;
        cross_build=$script:buildResult; boot_entry_enabled=$false;
        flash_performed=$false; artifacts=@($artifacts.ToArray())
    }
    $resultPath=Join-Path $script:runDirectory 'result.json'
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $resultPath -Encoding UTF8
    Write-M6Log "BUILD_ONLY_COMPLETE host_c=$($script:hostResult) cross_build=PASS result=$resultPath"
    Write-M6Log "REPORT_DIR=$script:runDirectory"
} catch {
    Write-M6Log "M6_BUILD_ONLY_STOPPED: $($_.Exception.Message) host_c=$($script:hostResult) cross_build=$($script:buildResult) no automatic rollback"
    if ($script:runDirectory) { Write-M6Log "REPORT_DIR=$script:runDirectory" }
    throw
}
