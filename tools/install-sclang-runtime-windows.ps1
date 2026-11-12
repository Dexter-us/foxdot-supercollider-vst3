param(
    [string]$RuntimeRoot = (Join-Path $env:RUNNER_TEMP 'dexter-sclang-runtime'),
    [string]$Bundle = (Join-Path $env:RUNNER_TEMP 'dexter-windows-vst3/plugin-build/VST3/Release/dexter_sc_tone.vst3')
)

$ErrorActionPreference = 'Stop'
$destination = Join-Path $Bundle 'Contents/Resources/SuperCollider'
foreach ($required in @(
    (Join-Path $RuntimeRoot 'sclang.exe'),
    (Join-Path $RuntimeRoot 'SCClassLibrary'),
    (Join-Path $RuntimeRoot 'SC_RUNTIME_VERSION.txt'),
    (Join-Path $RuntimeRoot 'Licenses/SuperCollider-COPYING.txt')
)) {
    if (-not (Test-Path $required)) {
        throw "Required sclang runtime package item is missing: $required"
    }
}
if (-not (Test-Path $Bundle)) {
    throw "Built VST3 bundle not found: $Bundle"
}
New-Item -ItemType Directory -Force -Path $destination | Out-Null
Copy-Item -Path (Join-Path $RuntimeRoot '*') -Destination $destination `
    -Recurse -Force
if (-not (Test-Path (Join-Path $destination 'sclang.exe')) -or
    -not (Test-Path (Join-Path $destination 'SCClassLibrary'))) {
    throw 'The sclang runtime was not installed into the VST3 resource bundle.'
}
Write-Host "Installed the pinned headless sclang runtime into $destination"
