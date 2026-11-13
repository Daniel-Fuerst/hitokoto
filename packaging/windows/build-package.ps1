# SPDX-License-Identifier: GPL-3.0-or-later
# S037: builds the Windows installer of one version on a Windows runner with Visual Studio 2022,
# the Qt of the recipe (on the path through jurplel/install-qt-action, QT_ROOT_DIR set) and the
# WiX .NET tool with WixToolset.UI.wixext.
#
#   pwsh packaging/windows/build-package.ps1 -Source <dir> -Version X.Y.Z -Out <dir>
#        -WixVersion X.Y.Z [-Ice ICEnn,...]
#
# <Source> holds the source of the tag vX.Y.Z (git archive); the recipe is the one next to this
# script (the commit the CI passed), applied to that source. The script configures in Release with
# the version given, builds, runs the tests (platform offscreen, benchmarks off), installs
# hitokoto.exe into a staging folder, adds what windeployqt deploys, the C++ runtime of the Visual
# Studio that built it, the licence and the third-party notice, checks the staged files against
# files.txt, and writes <Out>/hitokoto-X.Y.Z-x64.msi and <Out>/upgrade-test-0.0.0.msi (the same
# files plus a marker file, product version 0.0.0, for the upgrade test of install-check.ps1; never
# released). Both MSIs are validated (wix msi validate); the ICEs of -Ice are accepted.
param(
    [Parameter(Mandatory)] [string] $Source,
    [Parameter(Mandatory)] [string] $Version,
    [Parameter(Mandatory)] [string] $Out,
    [Parameter(Mandatory)] [string] $WixVersion,
    [string[]] $Ice = @()
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

if ($Version -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') {
    throw "version '$Version' is not MAJOR.MINOR.PATCH"
}
if (-not $env:QT_ROOT_DIR) { throw 'QT_ROOT_DIR is not set (install Qt first)' }

$recipe = $PSScriptRoot
$root = (Resolve-Path (Join-Path $recipe '..\..')).Path
$script = Join-Path $root 'scripts\packaging\WindowsPackage.cmake'
$Source = (Resolve-Path $Source).Path
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$Out = (Resolve-Path $Out).Path
$work = Join-Path ([IO.Path]::GetTempPath()) "hitokoto-msi-$Version"
if (Test-Path $work) { Remove-Item -Recurse -Force $work }
$build = Join-Path $work 'build'
$installed = Join-Path $work 'installed'
$stage = Join-Path $work 'stage'
New-Item -ItemType Directory -Force -Path $build, $installed, $stage | Out-Null

# Decision 4: Release with the version of the tag, tests on, benchmarks and fuzzers off.
cmake -S $Source -B $build -G 'Visual Studio 17 2022' -A x64 `
    "-DHITOKOTO_VERSION=$Version" `
    -DHITOKOTO_BUILD_TESTS=ON `
    -DHITOKOTO_BUILD_BENCHMARKS=OFF `
    -DHITOKOTO_BUILD_FUZZERS=OFF `
    -DHITOKOTO_WARNINGS_AS_ERRORS=OFF
cmake --build $build --config Release --parallel
$env:QT_QPA_PLATFORM = 'offscreen'
ctest --test-dir $build -C Release --output-on-failure
Remove-Item Env:QT_QPA_PLATFORM

# Decision 5: the program, alone; nothing else may come along (an install rule of a dependency).
cmake --install $build --config Release --prefix $installed
$installedFiles = @(Get-ChildItem -Recurse -File $installed |
    ForEach-Object { $_.FullName.Substring($installed.Length + 1) })
if ($installedFiles.Count -ne 1 -or $installedFiles[0] -ne 'bin\hitokoto.exe') {
    throw "cmake --install installed more or other than bin\hitokoto.exe: $($installedFiles -join ', ')"
}
Copy-Item (Join-Path $installed 'bin\hitokoto.exe') $stage

# Qt's part, without translations, compiler runtime, software OpenGL, shader compilers and the
# plugin types hitokoto does not use (they would bring Qt6Network).
& (Join-Path $env:QT_ROOT_DIR 'bin\windeployqt.exe') --release --no-translations `
    --no-compiler-runtime --no-opengl-sw --no-system-d3d-compiler --no-system-dxc-compiler `
    --skip-plugin-types generic,networkinformation,tls (Join-Path $stage 'hitokoto.exe')

# The C++ runtime of the Visual Studio that built it, beside the program (application-local).
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
$redistVersion = (Get-Content (Join-Path $vs 'VC\Auxiliary\Build\Microsoft.VCRedistVersion.default.txt')).Trim()
$crt = Join-Path $vs "VC\Redist\MSVC\$redistVersion\x64\Microsoft.VC143.CRT"
if (-not (Test-Path $crt)) { throw "no C++ runtime at $crt" }
Write-Host "C++ runtime from $crt"
Copy-Item (Join-Path $crt '*.dll') $stage

# The licence and the third-party notice (decision 5).
Copy-Item (Join-Path $Source 'LICENSE') (Join-Path $stage 'LICENSE.txt')
Copy-Item (Join-Path $recipe 'LGPL-3.0.txt') $stage
$qtVersion = Split-Path -Leaf (Split-Path -Parent $env:QT_ROOT_DIR)
cmake -DMODE=notice "-DTEMPLATE=$recipe\third-party.txt.in" "-DQT_VERSION=$qtVersion" `
    "-DOUTPUT=$stage\THIRD-PARTY.txt" -P $script

# The staged files against the recipe's list (a file more or less is red).
$list = Join-Path $work 'staged.txt'
Get-ChildItem -Recurse -File $stage | ForEach-Object { $_.FullName.Substring($stage.Length + 1) } |
    Set-Content -Encoding utf8 $list
cmake -DMODE=files "-DEXPECTED_FILE=$recipe\files.txt" "-DACTUAL_FILE=$list" -P $script

# The licence dialog shows the GPL as RTF.
cmake -DMODE=rtf "-DINPUT=$Source\LICENSE" "-DOUTPUT=$work\license.rtf" -P $script

$eula = 'wix' + $WixVersion.Split('.')[0]
function Build-Msi([string] $staged, [string] $productVersion, [string] $msi) {
    wix build (Join-Path $recipe 'hitokoto.wxs') -loc (Join-Path $recipe 'hitokoto.wxl') `
        -arch x64 -ext WixToolset.UI.wixext `
        -acceptEula $eula -d "Version=$productVersion" -bindpath "stage=$staged" `
        -bindpath "source=$Source" -bindpath "work=$work" -o $msi
    $suppress = @()
    foreach ($id in $Ice) { if ($id) { $suppress += @('-sice', $id) } }
    wix msi validate @suppress $msi
}

Build-Msi $stage $Version (Join-Path $Out "hitokoto-$Version-x64.msi")

# The upgrade test (review O4): the same files plus a marker that the upgrade must remove.
$testStage = Join-Path $work 'stage-upgrade-test'
Copy-Item -Recurse $stage $testStage
Set-Content -Encoding ascii (Join-Path $testStage 'upgrade-marker.txt') 'removed by the upgrade'
Build-Msi $testStage '0.0.0' (Join-Path $Out 'upgrade-test-0.0.0.msi')

Get-ChildItem $Out | Format-Table Name, Length
