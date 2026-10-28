param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("prepare", "configure-sc", "build-sc", "configure-plugin", "build-plugin", "package")]
    [string]$Stage,
    [string]$BuildRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    $BuildRoot = Join-Path $env:TEMP "Dexter-SuperCollider-VST3"
}

$buildLog = Join-Path $env:RUNNER_TEMP "dexter-windows-vst3-$Stage.log"
$buildScript = Join-Path $PSScriptRoot "build-windows-vst3.ps1"

Write-Host "Starting Windows VST3 build stage '$Stage'."
Write-Host "Stage log: $buildLog"

try {
    & $buildScript -BuildRoot $BuildRoot -Stage $Stage *>&1 |
        Tee-Object -FilePath $buildLog
    $buildExitCode = $LASTEXITCODE
    if ($buildExitCode -ne 0) {
        throw "Build stage '$Stage' exited with code $buildExitCode."
    }
}
catch {
    Write-Host "Stage '$Stage' failed. Last 120 log lines:"
    if (Test-Path $buildLog) {
        Get-Content $buildLog -Tail 120
    }
    throw
}
