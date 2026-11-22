[CmdletBinding()]
param(
    [string]$ProjectRoot = 'C:\pj\pd_pps',
    [string]$ScanPath = ''
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (-not $ScanPath) { $ScanPath = Join-Path $ProjectRoot 'scripts\project' }
if (-not (Test-Path -LiteralPath $ScanPath)) { throw "Missing scan path: $ScanPath" }
if (Test-Path -LiteralPath $ScanPath -PathType Leaf) {
    $files = @(Get-Item -LiteralPath $ScanPath)
} else { $files = @(Get-ChildItem -LiteralPath $ScanPath -Filter '*.ps1' -File -Recurse) }
if ($files.Count -eq 0) { throw "No PowerShell scripts found: $ScanPath" }
$violations = New-Object System.Collections.ArrayList
$shadowedExecutables = @('git','python','make','gcc','clang','cc','powershell','pwsh')
$protectedVariables = @('host','pid','pshome','true','false','null','psversiontable')
foreach ($sourceFile in $files) {
    $parseTokens = $null
    $parseErrors = $null
    $tree = [System.Management.Automation.Language.Parser]::ParseFile($sourceFile.FullName,[ref]$parseTokens,[ref]$parseErrors)
    foreach ($parseError in @($parseErrors)) {
        [void]$violations.Add("$($sourceFile.Name): SYNTAX $($parseError.Message)")
    }
    $functions = @($tree.FindAll({param($ast) $ast -is [System.Management.Automation.Language.FunctionDefinitionAst]},$true))
    foreach ($fn in $functions) {
        if ($shadowedExecutables -contains $fn.Name.ToLowerInvariant()) {
            [void]$violations.Add("$($sourceFile.Name): FUNCTION_SHADOWS_EXECUTABLE=$($fn.Name)")
        }
    }
    $assignments = @($tree.FindAll({param($ast) $ast -is [System.Management.Automation.Language.AssignmentStatementAst]},$true))
    foreach ($assignment in $assignments) {
        $leftVariable = $assignment.Left
        if ($leftVariable -is [System.Management.Automation.Language.VariableExpressionAst]) {
            $variableName = $leftVariable.VariablePath.UserPath.ToLowerInvariant()
            if ($protectedVariables -contains $variableName) {
                [void]$violations.Add("$($sourceFile.Name): ASSIGN_PROTECTED_VARIABLE=`$$variableName")
            }
        }
    }
    Write-Host "SCRIPT_PARSE_CHECK=$($sourceFile.FullName)"
}
foreach ($issue in $violations) { Write-Host "SCRIPT_SAFETY_FAIL=$issue" }
if ($violations.Count -gt 0) { throw "PowerShell safety check failed: $($violations.Count) issues" }
Write-Host "POWERSHELL_SAFETY_PASS scripts=$($files.Count) (AST static scope; Windows execution is separate)"
