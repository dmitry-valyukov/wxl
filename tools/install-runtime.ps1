# Installs the experimental Windows App Runtime on this machine, for the current
# user: the framework package, the main and singleton packages and the
# dynamic-dependency lifetime manager, from the installers of the
# Microsoft.WindowsAppSDK.Runtime package that the restore of the tree's build
# resolved. The package and its directory are read from the list of that restore,
# <build>\wxl.packages\restored-packages.json, which the configure writes
# (cmake/wxl_packages.cmake), so this script runs after a configure.
#
#   cmake --preset x64                                # in a developer prompt; tools\build.ps1 configures as well
#   pwsh -NoProfile -File tools\install-runtime.ps1   # -Packages <list> for a build directory other than build\x64
#
# Why: the WinUI metadata the wrappers are generated from is the experimental
# channel's, and only the experimental framework package (Microsoft.WindowsAppRuntime
# .2-experimentalF) has what it declares -- the stable runtime answers a call to
# WrapPanel, to the Window size properties or to TitleBar.RecomputeDragRegions with
# "no such interface". An application started by wxl asks for that framework by its
# version tag, WINDOWSAPPSDK_RELEASE_VERSION_TAG_W of include\WindowsAppSDK-VersionInfo.h
# in the same Runtime package, which wxl.ui/src/impl/bootstrap.cpp hands the
# bootstrapper. Installed next to the stable runtime, not instead of it: other
# applications keep theirs.
#
# Every run hands each installer to Add-AppxPackage; Windows leaves a package already
# installed at the same version as it is.
[CmdletBinding()]
param([string]$Packages = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build\x64\wxl.packages\restored-packages.json'))

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Packages)) {
    throw "No list of the restored packages at $Packages -- configure the tree first: its configure writes the list"
}
$runtime = (Get-Content -LiteralPath $Packages -Raw | ConvertFrom-Json).packages |
    Where-Object { $_.id -eq 'Microsoft.WindowsAppSDK.Runtime' }
if (-not $runtime) { throw "The restore of $Packages resolved no Microsoft.WindowsAppSDK.Runtime" }

$arch = switch ($env:PROCESSOR_ARCHITECTURE) { 'ARM64' { 'win10-arm64' } 'x86' { 'win10-x86' } default { 'win10-x64' } }
$dir = Join-Path $runtime.directory "tools\MSIX\$arch"
if (-not (Test-Path $dir)) { throw "No installers at $dir, in Microsoft.WindowsAppSDK.Runtime $($runtime.version)" }

# The order Windows App SDK's own installer uses: the framework first, the
# packages that depend on it after.
foreach ($pattern in 'Microsoft.WindowsAppRuntime.2-*.msix', 'Microsoft.WindowsAppRuntime.Main.2-*.msix',
                     'Microsoft.WindowsAppRuntime.Singleton.2-*.msix', 'Microsoft.WindowsAppRuntime.DDLM.2-*.msix') {
    $file = Get-ChildItem $dir -Filter $pattern | Select-Object -First 1
    if (-not $file) { throw "No $pattern in $dir" }
    "install $($file.Name)"
    Add-AppxPackage -Path $file.FullName
}
Get-AppxPackage -Name 'Microsoft.WindowsAppRuntime.2-experimental*' | Select-Object Name, Version
