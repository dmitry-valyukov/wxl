# Generates the WinUI3 cppwinrt projection the build needs in generated/.
#
# The tree does not carry it: generated/ is ignored by git. The generator does
# not need it, wxl.ui does: once the wrappers are generated and wxl.ui is
# configured, it stops the configure with a fatal error when
# winrt/Microsoft.UI.Xaml.h is not there. This is the step that produces it; on
# a fresh clone it comes right after tools\restore-packages.ps1.
#
# Microsoft.UI.Xaml.winmd alone is not enough. It reaches into three other
# WindowsAppSDK packages and into WebView2, and cppwinrt resolves every type
# it can see whether or not that type is wanted in the output -- so all of
# them are inputs here.
#
#   pwsh -NoProfile -File tools\projection.ps1                 # into <repo>\generated
#   pwsh -NoProfile -File tools\projection.ps1 -Out <dir>
#
# Versions are the pins of the tree, not whatever the NuGet cache holds: the
# packages of the Windows App SDK release named in wxl.gen/profiles/base.json at
# the versions that release declares, and the "extra" packages of
# tools/packages.json. tools/restore-packages.ps1 puts them all into the cache;
# pass a version here to try another.
#
# Nothing re-runs this: after the pins change, run restore-packages.ps1 and then
# this script again by hand.
[CmdletBinding()]
param(
    [string]$Out,
    [string]$NuGetRoot,
    [string]$WinUIVersion,
    [string]$InteractiveExperiencesVersion,
    [string]$FoundationVersion,
    [string]$WebView2Version,
    [string]$Win2DVersion
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $Out) { $Out = Join-Path $repo 'generated' }

. (Join-Path $PSScriptRoot 'nuget-root.ps1')
. (Join-Path $PSScriptRoot 'windows-app-sdk.ps1')
function Resolve-Root { Resolve-NuGetRoot $NuGetRoot }

# The pinned version of a package, and a check that the restore put it there.
function Resolve-Version([string]$package, [string]$pinned) {
    if (-not $pinned) { throw "No pinned version for $package" }
    $dir = Join-Path (Resolve-Root) "$package\$pinned"
    if (-not (Test-Path $dir)) { throw "$package $pinned is not in the NuGet cache ($dir); run tools\restore-packages.ps1" }
    $pinned
}

$manifest = Get-Content (Join-Path $PSScriptRoot 'packages.json') -Raw | ConvertFrom-Json
$declared = Get-WindowsAppSdkPackageVersion (Resolve-Root) (Get-WindowsAppSdkRelease $repo)
function Pin([string]$id) {
    $x = @($manifest.extra | Where-Object { $_.id -eq $id })
    if ($x) { return $x[0].version }
    $declared[$id.ToLowerInvariant()]
}

# cppwinrt.exe ships inside vcpkg's cppwinrt port. VCPKG_ROOT is read from
# vcpkg's own path file first, because entering a Visual Studio developer
# shell overwrites the environment variable with VS's bundled vcpkg, which
# has no cppwinrt port -- the same reason cmake/vcpkg_bootstrap.cmake does it
# that way.
function Resolve-CppWinRT {
    $roots = @()
    $pathFile = Join-Path $env:USERPROFILE 'Local Settings\vcpkg\vcpkg.path.txt'
    if (Test-Path $pathFile) { $roots += (Get-Content $pathFile -Raw).Trim() }
    if ($env:VCPKG_ROOT) { $roots += $env:VCPKG_ROOT }
    foreach ($r in $roots) {
        $exe = Join-Path $r 'installed\x64-windows-static-md\tools\cppwinrt\cppwinrt.exe'
        if (Test-Path $exe) { return $exe }
    }
    throw "cppwinrt.exe not found. Install it: vcpkg install cppwinrt:x64-windows-static-md"
}

$nuget = Resolve-Root
$winui = Resolve-Version 'microsoft.windowsappsdk.winui' ($(if ($WinUIVersion) { $WinUIVersion } else { Pin 'Microsoft.WindowsAppSDK.WinUI' }))
$ie = Resolve-Version 'microsoft.windowsappsdk.interactiveexperiences' ($(if ($InteractiveExperiencesVersion) { $InteractiveExperiencesVersion } else { Pin 'Microsoft.WindowsAppSDK.InteractiveExperiences' }))
$foundation = Resolve-Version 'microsoft.windowsappsdk.foundation' ($(if ($FoundationVersion) { $FoundationVersion } else { Pin 'Microsoft.WindowsAppSDK.Foundation' }))
$webview = Resolve-Version 'microsoft.web.webview2' ($(if ($WebView2Version) { $WebView2Version } else { Pin 'Microsoft.Web.WebView2' }))
# Win2D (Microsoft.Graphics.Canvas): async loading of images to video memory and
# drawing them into composition surfaces. Consumed raw by wxl's own facilities
# (the texture cache of CompositionWindow); no wrappers are generated for it.
# Its winmd sits under lib\uap10.0, not a WindowsAppSDK metadata folder.
$win2d = Resolve-Version 'microsoft.graphics.win2d' ($(if ($Win2DVersion) { $Win2DVersion } else { Pin 'Microsoft.Graphics.Win2D' }))

# The interactive-experiences package ships one metadata folder per Windows
# contract version; the newest is the one whose types the rest resolves to.
$ieMetadata = Get-ChildItem (Join-Path $nuget "microsoft.windowsappsdk.interactiveexperiences\$ie\metadata") -Directory |
    ForEach-Object { $v = $null; if ([version]::TryParse($_.Name, [ref]$v)) { [pscustomobject]@{ V = $v; Path = $_.FullName } } } |
    Sort-Object V | Select-Object -Last 1 -ExpandProperty Path

$inputs = @(
    Join-Path $nuget "microsoft.windowsappsdk.winui\$winui\metadata\Microsoft.UI.Xaml.winmd"
    Join-Path $nuget "microsoft.windowsappsdk.winui\$winui\metadata\Microsoft.UI.Text.winmd"
    $ieMetadata
    Join-Path $nuget "microsoft.windowsappsdk.foundation\$foundation\metadata"
    Join-Path $nuget "microsoft.web.webview2\$webview\lib\Microsoft.Web.WebView2.Core.winmd"
    Join-Path $nuget "microsoft.graphics.win2d\$win2d\lib\uap10.0\Microsoft.Graphics.Canvas.winmd"
)
foreach ($i in $inputs) { if (-not (Test-Path $i)) { throw "Metadata missing: $i" } }

$cppwinrt = Resolve-CppWinRT
"cppwinrt : $cppwinrt"
"packages : winui $winui, interactiveexperiences $ie, foundation $foundation, webview2 $webview, win2d $win2d"
"output   : $Out"

# Not $args: that name is PowerShell's own automatic variable.
$cppwinrtArgs = @()
foreach ($i in $inputs) { $cppwinrtArgs += @('-input', $i) }
$cppwinrtArgs += @('-reference', 'sdk', '-output', $Out)
& $cppwinrt @cppwinrtArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

"wrote $((Get-ChildItem (Join-Path $Out 'winrt') -File).Count) namespace headers into $Out\winrt"
