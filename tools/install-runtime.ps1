# Installs the experimental Windows App Runtime on this machine, for the current
# user: the framework package, the main and singleton packages and the
# dynamic-dependency lifetime manager, from the installers
# tools/restore-packages.ps1 fetched into the NuGet package folder.
#
#   pwsh -NoProfile -File tools\restore-packages.ps1     # once, fetches them
#   pwsh -NoProfile -File tools\install-runtime.ps1
#
# Why: the WinUI metadata the wrappers are generated from is the experimental
# channel's, and only the experimental framework package (Microsoft.WindowsAppRuntime
# .2-experimentalF) has what it declares -- the stable runtime answers a call to
# WrapPanel, to the Window size properties or to TitleBar.RecomputeDragRegions with
# "no such interface". An application started by wxl asks for that framework by its
# version tag (WXL_WINDOWSAPPSDK_VERSION_TAG in wxl.ui/CMakeLists.txt). Installed
# next to the stable runtime, not instead of it: other applications keep theirs.
#
# A package already installed at this version is left alone, so a second run is a no-op.
[CmdletBinding()]
param([string]$NuGetRoot)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'nuget-root.ps1')
$root = Resolve-NuGetRoot $NuGetRoot
$manifest = Get-Content (Join-Path $PSScriptRoot 'packages.json') -Raw | ConvertFrom-Json
$runtime = @($manifest.experimental | Where-Object { $_.id -eq 'Microsoft.WindowsAppSDK.Runtime' })[0]
if (-not $runtime) { throw 'tools/packages.json has no Microsoft.WindowsAppSDK.Runtime entry' }

$arch = switch ($env:PROCESSOR_ARCHITECTURE) { 'ARM64' { 'win10-arm64' } 'x86' { 'win10-x86' } default { 'win10-x64' } }
$dir = Join-Path $root "microsoft.windowsappsdk.runtime\$($runtime.version)\tools\MSIX\$arch"
if (-not (Test-Path $dir)) { throw "No installers at $dir -- run tools\restore-packages.ps1 first" }

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
