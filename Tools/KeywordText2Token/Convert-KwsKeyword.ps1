param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Text,

    [string]$InputFile,
    [string]$OutputFile,
    [string]$ModelDir,
    [string]$Uv = "uv",
    [string]$Python
)

$ErrorActionPreference = "Stop"
$ToolDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Script = Join-Path $ToolDir "kws_text2token.py"
$CacheDir = Join-Path $ToolDir ".uv-cache"
$PythonInstallDir = Join-Path $ToolDir ".uv-python"

if (-not (Get-Command $Uv -ErrorAction SilentlyContinue)) {
    throw "uv was not found. Install uv or pass -Uv <path-to-uv.exe>."
}

$env:UV_PYTHON_INSTALL_DIR = $PythonInstallDir

$Args = @("--cache-dir", $CacheDir, "run", "--project", $ToolDir)
if ($Python) {
    $Args += @("--python", $Python)
}
$Args += @("python", $Script)
if ($InputFile) {
    $Args += @("--input", $InputFile)
}
if ($OutputFile) {
    $Args += @("--output", $OutputFile)
}
if ($ModelDir) {
    $Args += @("--model-dir", $ModelDir)
}
foreach ($Item in $Text) {
    $Args += @("--text", $Item)
}

& $Uv @Args
exit $LASTEXITCODE
