param(
    [string]$BuildRoot = (Join-Path $env:RUNNER_TEMP 'dexter-windows-vst3'),
    [string]$RuntimeRoot = (Join-Path $env:RUNNER_TEMP 'dexter-sclang-runtime')
)

$ErrorActionPreference = 'Stop'
$expectedCommit = 'ef5771bce3d1c9c780cafd58cf032ec4a3f36d10'
$source = Join-Path $BuildRoot 'deps/supercollider'
$build = Join-Path $BuildRoot 'supercollider-build'

if (-not (Test-Path (Join-Path $source '.git'))) {
    throw "The pinned SuperCollider source is missing: $source. Run the cache-backed VST3 build step first."
}
if (-not (Test-Path (Join-Path $build 'CMakeCache.txt'))) {
    throw "The existing SuperCollider build cache is missing: $build"
}

$actualCommit = (& git -C $source rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualCommit -ne $expectedCommit) {
    throw "Expected pinned SuperCollider commit $expectedCommit, found $actualCommit."
}

$cacheText = Get-Content (Join-Path $build 'CMakeCache.txt') -Raw
if ($cacheText -notmatch 'CMAKE_GENERATOR:INTERNAL=Visual Studio') {
    throw 'The restored SuperCollider build cache is not a Visual Studio build.'
}

Write-Host "Configuring the pinned 3.15 language helper with SCLANG_SERVER=OFF."
Write-Host 'This builds only the sclang language target; it does not rebuild scsynth or UGen modules.'
& cmake -S $source -B $build `
    -DSCLANG_SERVER=OFF `
    -DSC_QT=OFF `
    -DSC_IDE=OFF
if ($LASTEXITCODE -ne 0) {
    throw "SuperCollider language-only CMake configuration failed ($LASTEXITCODE)."
}
$configuredCache = Get-Content (Join-Path $build 'CMakeCache.txt') -Raw
if ($configuredCache -notmatch 'SCLANG_SERVER:BOOL=OFF') {
    throw 'CMake did not apply SCLANG_SERVER=OFF; refusing to build an internal audio server.'
}

Write-Host 'Building only the sclang target from the restored 3.15 build tree.'
& cmake --build $build --config Release --target sclang -- /m:2
if ($LASTEXITCODE -ne 0) {
    throw "The sclang target failed to build ($LASTEXITCODE)."
}

$releaseSclang = Join-Path $build 'lang/Release/sclang.exe'
$sclang = if (Test-Path $releaseSclang) {
    Get-Item $releaseSclang
} else {
    Get-ChildItem -Path (Join-Path $build 'lang') `
        -Filter 'sclang.exe' -File -Recurse |
        Select-Object -First 1
}
if (-not $sclang) {
    throw 'The sclang target reported success but did not produce sclang.exe.'
}

if (Test-Path $RuntimeRoot) {
    Remove-Item -LiteralPath $RuntimeRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $RuntimeRoot | Out-Null

& cmake `
    "-DSCLANG_EXE=$($sclang.FullName)" `
    "-DSCLANG_SOURCE=$source" `
    "-DSCLANG_BUILD=$build" `
    "-DSCLANG_OUTPUT=$RuntimeRoot" `
    -P (Join-Path $PSScriptRoot 'package-sclang-runtime.cmake')
if ($LASTEXITCODE -ne 0) {
    throw "Could not package the sclang runtime dependencies ($LASTEXITCODE)."
}

$classLibrary = Join-Path $RuntimeRoot 'SCClassLibrary'
if (-not (Test-Path (Join-Path $RuntimeRoot 'sclang.exe')) -or
    -not (Test-Path $classLibrary)) {
    throw 'The packaged sclang runtime is missing its executable or class library.'
}

$licenseDirectory = Join-Path $RuntimeRoot 'Licenses'
New-Item -ItemType Directory -Force -Path $licenseDirectory | Out-Null
Copy-Item (Join-Path $source 'COPYING') `
    (Join-Path $licenseDirectory 'SuperCollider-COPYING.txt') -Force
foreach ($dependency in @('boost', 'yaml-cpp', 'icu')) {
    $dependencyRoot = Join-Path $source "external_libraries/$dependency"
    if (-not (Test-Path $dependencyRoot)) { continue }
    $dependencyLicenses = Get-ChildItem -Path $dependencyRoot -File -Recurse `
        -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -match '^(LICENSE|LICENCE|COPYING|COPYRIGHT)([._-].*)?$' -or
            $_.Name -match '^license\.(html|txt|md)$'
        }
    foreach ($dependencyLicense in $dependencyLicenses) {
        $targetName = "$dependency-$($dependencyLicense.Name)"
        Copy-Item $dependencyLicense.FullName `
            (Join-Path $licenseDirectory $targetName) -Force
    }
}

$sourceVersion = '3.15.0'
$versionFile = Join-Path $source 'SCVersion.txt'
if (Test-Path $versionFile) {
    $versionText = Get-Content $versionFile -Raw
    if ($versionText -match '3\.15(?:\.\d+)?') {
        $sourceVersion = $Matches[0]
    }
}
$runtimeManifest = @"
SuperCollider language runtime
Version: $sourceVersion
Source commit: $expectedCommit
API generation: 7
Build option: SCLANG_SERVER=OFF
Audio engine: the VST's existing embedded scsynth World
Network: loopback-only OSC control

This package contains sclang and its class library. It does not include a
second scsynth engine or rebuild the VST's SuperCollider/UGen binaries.
"@
Set-Content -LiteralPath (Join-Path $RuntimeRoot 'SC_RUNTIME_VERSION.txt') `
    -Value $runtimeManifest -Encoding UTF8

# Smoke-test that the packaged interpreter starts and compiles the bundled
# class library without booting an audio server.
$smokeDirectory = Join-Path $env:RUNNER_TEMP 'dexter-sclang-smoke'
New-Item -ItemType Directory -Force -Path $smokeDirectory | Out-Null
$config = @"
includePaths:
  - "$(($classLibrary -replace '\\', '/').Replace('"', '\"'))"
excludePaths: []
excludeDefaultPaths: true
"@
$configPath = Join-Path $smokeDirectory 'sclang_conf.yaml'
$smokePath = Join-Path $smokeDirectory 'smoke.scd'
[System.IO.File]::WriteAllText(
    $configPath, $config, [System.Text.UTF8Encoding]::new($false))
[System.IO.File]::WriteAllText(
    $smokePath, '"DEXTER_SCLANG_SMOKE_OK".postln; 0.exit;',
    [System.Text.UTF8Encoding]::new($false))
$stdout = Join-Path $smokeDirectory 'stdout.log'
$stderr = Join-Path $smokeDirectory 'stderr.log'
$quotedConfig = '"' + $configPath + '"'
$quotedSmoke = '"' + $smokePath + '"'
$arguments = "-D -u 0 -l $quotedConfig $quotedSmoke"
$process = Start-Process -FilePath (Join-Path $RuntimeRoot 'sclang.exe') `
    -ArgumentList $arguments -WorkingDirectory $RuntimeRoot -NoNewWindow `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
if (-not $process.WaitForExit(120000)) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    throw 'Packaged sclang did not finish its headless startup smoke test in 120 seconds.'
}
$smokeOutput = ''
if (Test-Path $stdout) { $smokeOutput += Get-Content $stdout -Raw }
if (Test-Path $stderr) { $smokeOutput += Get-Content $stderr -Raw }
if ($process.ExitCode -ne 0 -or
    $smokeOutput -notmatch 'DEXTER_SCLANG_SMOKE_OK') {
    Write-Host $smokeOutput
    throw "Packaged sclang smoke test failed with exit code $($process.ExitCode)."
}
Write-Host 'Packaged sclang 3.15 runtime smoke test passed.'
