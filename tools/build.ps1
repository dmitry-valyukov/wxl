# Configures and builds the tree from a plain shell.
#
# The presets need Ninja and cl.exe on PATH, which normally means starting
# from a Developer Command Prompt. This script imports that environment
# itself: it runs vcvars64.bat in a child cmd, reads back the variables it
# set, and applies them to the current process.
#
# The same environment names the Visual Studio installation whose MSBuild the
# configure runs to restore the NuGet packages and build the C++/WinRT
# projection (VSINSTALLDIR), and the Windows SDK of both the projection and the
# compiler (WindowsSDKVersion): see cmake/wxl_packages.cmake.
#
# Usage:
#   tools\build.ps1                 # configure (if needed) and build Debug
#   tools\build.ps1 -Config Release
#   tools\build.ps1 -Configure      # force a re-configure first
#   tools\build.ps1 -Target winui-srcgen
#   tools\build.ps1 -Configure -Define WXL_BUILD_BENCHMARKS=ON
#   tools\build.ps1 -Toolset 14.52  # another installed MSVC toolset; the default is pinned below

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Debug',
    [string]$Preset = 'x64',
    [string]$Target,
    [switch]$Configure,

    # The MSVC toolset vcvars64.bat is asked for (-vcvars_ver). Pinned here rather
    # than left to the machine's default: the tree names what it builds with, and
    # switching is one value, not three files under Program Files. Ignored when
    # the script runs inside a developer prompt already -- that prompt chose.
    [string]$Toolset = '14.53',

    # Cache entries passed on to the configure step, without the -D. They stick
    # in CMakeCache.txt, so a later run without them keeps whatever was set.
    [string[]]$Define
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

function Import-VisualStudioEnvironment {
    if ($env:VSCMD_VER) { return }  # already inside a developer prompt

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found: $vswhere" }

    # -prerelease so an Insiders/preview installation counts; the tree needs a
    # toolset new enough for `import std;`, which is where those tend to be.
    $install = & $vswhere -latest -prerelease -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $install) { throw 'No Visual Studio installation with the C++ toolset was found.' }

    $vcvars = Join-Path $install 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found: $vcvars" }

    & "$env:ComSpec" /c "`"$vcvars`" -vcvars_ver=$Toolset >nul 2>nul && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') {
            Set-Item -Path "env:$($Matches[1])" -Value $Matches[2]
        }
    }
}

Import-VisualStudioEnvironment

$buildDir = Join-Path $root "build\$Preset"
if ($Configure -or $Define -or -not (Test-Path (Join-Path $buildDir 'CMakeCache.txt'))) {
    $configureArgs = @('--preset', $Preset, '-S', $root)
    foreach ($d in $Define) { $configureArgs += "-D$d" }
    cmake @configureArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# The build directory by name rather than `--build --preset`: a build preset is
# looked up in the CMakePresets.json of whatever directory the shell happens to
# stand in, so calling this script by its full path from elsewhere -- which is
# what working in a git worktree amounts to -- would configure this tree and
# then build the other one. The two forms are otherwise the same: the presets
# here carry the build directory and the configuration and nothing else.
$buildArgs = @('--build', $buildDir, '--config', $Config)
if ($Target) { $buildArgs += @('--target', $Target) }
cmake @buildArgs
exit $LASTEXITCODE
