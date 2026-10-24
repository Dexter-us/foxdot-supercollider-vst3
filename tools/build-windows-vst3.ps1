param(
    [string]$BuildRoot,
    [int]$BuildJobs = 2
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    $BuildRoot = Join-Path $env:TEMP "Dexter-SuperCollider-VST3"
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$pluginSource = Join-Path $repoRoot "spikes/vst3-callback"
if (-not (Test-Path (Join-Path $pluginSource "CMakeLists.txt"))) {
    $pluginSource = $repoRoot
}

$scCommit = "ef5771bce3d1c9c780cafd58cf032ec4a3f36d10"
$vst3Tag = "v3.8.1_build_84"

$vswhere = Join-Path ([Environment]::GetFolderPath("ProgramFilesX86")) "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "Visual Studio 2022 or 2026 Build Tools are required for the Windows build."
}
$vsVersion = (& $vswhere -latest -products "*" -property installationVersion).Trim()
if ($LASTEXITCODE -ne 0 -or $vsVersion -notmatch "^(?<major>\d+)\.") {
    throw "Could not determine the installed Visual Studio version from vswhere."
}
switch ([int]$Matches["major"]) {
    17 { $vsGenerator = "Visual Studio 17 2022" }
    18 { $vsGenerator = "Visual Studio 18 2026" }
    default { throw "Unsupported Visual Studio version $vsVersion; use Visual Studio 2022 or 2026." }
}
$scSource = Join-Path $BuildRoot "deps/supercollider"
$scBuild = Join-Path $BuildRoot "supercollider-build"
$vst3Sdk = Join-Path $BuildRoot "deps/vst3sdk"
$pythonRuntime = Join-Path $BuildRoot "deps/python-3.12-embed"
$pluginBuild = Join-Path $BuildRoot "plugin-build"

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    Write-Host "`n> $Executable $($Arguments -join ' ')"
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Executable failed with exit code $LASTEXITCODE"
    }
}

foreach ($tool in @("git", "cmake")) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "Required command '$tool' was not found. Install Git, CMake, and Visual Studio 2022 or 2026 Build Tools."
    }
}

New-Item -ItemType Directory -Force -Path $BuildRoot | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $BuildRoot "deps") | Out-Null

$pythonVersion = "3.12.10"
$pythonArchive = Join-Path $BuildRoot "deps/python-3.12-embed-amd64.zip"
if (-not (Test-Path (Join-Path $pythonRuntime "python312.dll"))) {
    New-Item -ItemType Directory -Force -Path $pythonRuntime | Out-Null
    Invoke-WebRequest `
        -Uri "https://www.python.org/ftp/python/$pythonVersion/python-$pythonVersion-embed-amd64.zip" `
        -OutFile $pythonArchive
    Expand-Archive -Path $pythonArchive -DestinationPath $pythonRuntime -Force
}
if (-not (Test-Path (Join-Path $pythonRuntime "PYTHON-LICENSE.txt"))) {
    $packageLicense = Join-Path $pythonRuntime "LICENSE.txt"
    if (Test-Path $packageLicense) {
        Copy-Item $packageLicense (Join-Path $pythonRuntime "PYTHON-LICENSE.txt")
    }
    else {
        Invoke-WebRequest `
            -Uri "https://raw.githubusercontent.com/python/cpython/v$pythonVersion/LICENSE" `
            -OutFile (Join-Path $pythonRuntime "PYTHON-LICENSE.txt")
    }
}
if (-not (Test-Path (Join-Path $pythonRuntime "python312._pth"))) {
    throw "The CPython embeddable distribution is missing python312._pth."
}
if (-not (Test-Path (Join-Path $pythonRuntime "python312.zip"))) {
    throw "The CPython embeddable distribution is missing python312.zip."
}
if (-not (Test-Path (Join-Path $pythonRuntime "python312.dll"))) {
    throw "The CPython embeddable distribution is missing python312.dll."
}
if (-not (Test-Path (Join-Path $pythonRuntime "python3.dll"))) {
    throw "The CPython embeddable distribution is missing python3.dll."
}
if (-not (Test-Path (Join-Path $pythonRuntime "vcruntime140.dll"))) {
    throw "The CPython embeddable distribution is missing vcruntime140.dll."
}
$pythonExe = Join-Path $pythonRuntime "python.exe"
if (-not (Test-Path $pythonExe)) {
    throw "The CPython embeddable distribution is missing python.exe."
}
Write-Host "Prepared bundled CPython $pythonVersion runtime."

& $pythonExe -c "import sys, time, traceback; assert sys.version_info[:2] == (3, 12)"
if ($LASTEXITCODE -ne 0) {
    throw "The bundled CPython runtime did not pass its startup smoke test."
}
$patternApiTest = Join-Path $repoRoot "tools/test-pattern-api.py"
if (-not (Test-Path $patternApiTest)) {
    throw "The pattern API smoke test is missing: $patternApiTest"
}
Invoke-Checked $pythonExe @($patternApiTest, (Join-Path $pluginSource 'python_runtime.cpp'))

if (-not (Test-Path (Join-Path $scSource ".git"))) {
    if (Test-Path $scSource) {
        throw "The SuperCollider source path exists but is not a Git checkout: $scSource"
    }
    Invoke-Checked "git" @(
        "clone", "--recurse-submodules",
        "https://github.com/supercollider/supercollider.git", $scSource
    )
}
Invoke-Checked "git" @("-C", $scSource, "checkout", "--detach", $scCommit)
Invoke-Checked "git" @("-C", $scSource, "submodule", "update", "--init", "--recursive")

if (-not (Test-Path (Join-Path $vst3Sdk "CMakeLists.txt"))) {
    if (Test-Path $vst3Sdk) {
        throw "The VST3 SDK path exists but is not a checkout: $vst3Sdk"
    }
    Invoke-Checked "git" @(
        "clone", "--recurse-submodules", "--branch", $vst3Tag,
        "https://github.com/steinbergmedia/vst3sdk.git", $vst3Sdk
    )
}
Invoke-Checked "git" @("-C", $vst3Sdk, "checkout", "--detach", $vst3Tag)
Invoke-Checked "git" @("-C", $vst3Sdk, "submodule", "update", "--init", "--recursive")

$vcpkgRootWasSet = Test-Path Env:VCPKG_ROOT
$savedVcpkgRoot = $env:VCPKG_ROOT
Remove-Item Env:VCPKG_ROOT -ErrorAction SilentlyContinue
try {
    Invoke-Checked "cmake" @(
        "-S", $scSource,
        "-B", $scBuild,
        "-G", $vsGenerator,
        "-A", "x64",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
        "-DSC_QT=OFF",
        "-DSC_IDE=OFF",
        "-DSC_DOC_RENDER=OFF",
        "-DNO_LIBSNDFILE=ON",
        "-DFFT_GREEN=ON",
        "-DLIBSCSYNTH=OFF",
        "-DSTATIC_PLUGINS=OFF",
        "-DSUPERNOVA=OFF",
        "-DSC_HIDAPI=OFF",
        "-DSC_ABLETON_LINK=OFF",
        "-DENABLE_TESTSUITE=OFF",
        "-DNO_X11=ON",
        "-DAUDIOAPI=portaudio",
        "-DSYSTEM_PORTAUDIO=OFF"
    )
}
finally {
    if ($vcpkgRootWasSet) {
        $env:VCPKG_ROOT = $savedVcpkgRoot
    }
}

Invoke-Checked "cmake" @(
    "--build", $scBuild,
    "--config", "Release",
    "--target", "libscsynth", "OscUGens", "BinaryOpUGens", "IOUGens",
    "--", "/m:$BuildJobs"
)

$requiredModules = @("OscUGens.scx", "BinaryOpUGens.scx", "IOUGens.scx")
$moduleFiles = foreach ($module in $requiredModules) {
    $moduleCandidates = @(Get-ChildItem -Path $scBuild -Filter $module -File -Recurse)
    $releaseMatch = @($moduleCandidates | Where-Object { $_.FullName -match "[\\/]Release[\\/]" })
    if ($releaseMatch.Count -gt 0) {
        $match = $releaseMatch[0]
    }
    elseif ($moduleCandidates.Count -gt 0) {
        $match = $moduleCandidates[0]
    }
    else {
        throw "Could not find $module under the SuperCollider build directory: $scBuild"
    }
    $match
}

$moduleDirectories = @($moduleFiles | ForEach-Object { $_.DirectoryName } | Select-Object -Unique)
if ($moduleDirectories.Count -ne 1) {
    throw "The required UGen modules were built into different directories: $($moduleDirectories -join ', ')"
}
$ugenPluginPath = $moduleDirectories[0]

Invoke-Checked "cmake" @(
    "-S", $pluginSource,
    "-B", $pluginBuild,
    "-G", $vsGenerator,
    "-A", "x64",
    "-DVST3_SDK_ROOT=$vst3Sdk",
    "-DSC_SOURCE_DIR=$scSource",
    "-DSC_BUILD_DIR=$scBuild",
    "-DSC_UGEN_PLUGIN_PATH=$ugenPluginPath",
    "-DPYTHON_RUNTIME_DIR=$pythonRuntime"
)
Invoke-Checked "cmake" @(
    "--build", $pluginBuild,
    "--config", "Release",
    "--target", "dexter_sc_tone",
    "--", "/m:$BuildJobs"
)

$bundle = Get-ChildItem -Path (Join-Path $pluginBuild "VST3") -Filter "dexter_sc_tone.vst3" -Directory -Recurse |
    Select-Object -First 1
if (-not $bundle) {
    throw "The VST3 bundle was not produced under $pluginBuild"
}

$portableDirectory = Join-Path $bundle.FullName "Portable"
New-Item -ItemType Directory -Force -Path $portableDirectory | Out-Null
$portableSource = Join-Path $pluginSource "Portable"
if (-not (Test-Path $portableSource)) {
    throw "Portable examples folder is missing: $portableSource"
}
Get-ChildItem -Path $portableSource -Force |
    Copy-Item -Destination $portableDirectory -Recurse -Force

$resourceRoot = Join-Path $bundle.FullName "Contents/Resources"
foreach ($module in $requiredModules) {
    if (-not (Test-Path (Join-Path $resourceRoot "SCPlugins/$module"))) {
        throw "The VST3 bundle is missing UGen module $module"
    }
}
foreach ($license in @("Licenses/COPYING", "Licenses/LICENSE.txt")) {
    if (-not (Test-Path (Join-Path $resourceRoot $license))) {
        throw "The VST3 bundle is missing license file $license"
    }
}
foreach ($pythonFile in @("Python/python312.dll", "Python/python3.dll", "Python/python312.zip",
        "Python/python312._pth", "Python/vcruntime140.dll")) {
    if (-not (Test-Path (Join-Path $resourceRoot $pythonFile))) {
        throw "The VST3 bundle is missing portable Python runtime file $pythonFile"
    }
}
if (-not (Test-Path (Join-Path $resourceRoot "Licenses/PYTHON-LICENSE.txt"))) {
    throw "The VST3 bundle is missing Python's license file"
}
if (-not (Test-Path (Join-Path $portableDirectory "README.txt"))) {
    throw "The VST3 bundle is missing its Portable scripts folder"
}
$portableExamples = @(Get-ChildItem -Path $portableDirectory -Filter "*.py" -File)
if ($portableExamples.Count -lt 6) {
    throw "Expected at least six packaged starter Python scripts in Portable; found $($portableExamples.Count)."
}

$validator = Get-ChildItem -Path $pluginBuild -Filter "validator.exe" -File -Recurse |
    Select-Object -First 1
if (-not $validator) {
    throw "The VST3 validator executable was not produced under $pluginBuild"
}
Invoke-Checked $validator.FullName @($bundle.FullName)

Write-Host "`nWindows VST3 build and validator completed successfully."
Write-Host "Bundle: $($bundle.FullName)"
