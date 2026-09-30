# Fetches the NuGet packages the tree builds from into the NuGet package
# folder -- the one step a clean machine needs before `projection.ps1`, the
# generator and CMake, and the only one that touches the network (nuget.org).
#
#   pwsh -NoProfile -File tools\restore-packages.ps1
#   pwsh -NoProfile -File tools\restore-packages.ps1 -NuGetRoot D:\.nuget
#
# What is fetched:
#  - the Windows App SDK release named by "windowsAppSdk" in
#    wxl.gen/profiles/base.json, and the packages of tools/packages.json's
#    "sdkPackages" at the versions that release declares;
#  - the pinned "extra" and "experimental" packages of tools/packages.json.
# A package already in the folder is left alone, so a second run is a no-op.
# The layout is NuGet's own (<root>\<id lower case>\<version>\), which is what
# every other script and the generator read.
[CmdletBinding()]
param([string]$NuGetRoot)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'nuget-root.ps1')
$repo = Split-Path -Parent $PSScriptRoot
$root = Resolve-NuGetRoot $NuGetRoot
$manifest = Get-Content (Join-Path $PSScriptRoot 'packages.json') -Raw | ConvertFrom-Json
$flat = 'https://api.nuget.org/v3-flatcontainer'

function Get-Package([string]$id, [string]$version) {
    $low = $id.ToLowerInvariant()
    $dir = Join-Path $root "$low\$version"
    if (Test-Path (Join-Path $dir "$low.nuspec")) { "have    $id $version"; return }

    $url = "$flat/$low/$version/$low.$version.nupkg"
    $file = Join-Path ([IO.Path]::GetTempPath()) "$low.$version.nupkg"
    "fetch   $id $version  <- $url"
    Invoke-WebRequest -Uri $url -OutFile $file -UseBasicParsing
    try {
        New-Item -ItemType Directory -Force $dir | Out-Null
        Expand-Archive -LiteralPath $file -DestinationPath $dir -Force
    } finally {
        Remove-Item $file -ErrorAction SilentlyContinue
    }
    if (-not (Test-Path (Join-Path $dir "$low.nuspec"))) { throw "$id $version unpacked without a nuspec into $dir" }
}

"package folder: $root"

$base = Get-Content (Join-Path $repo 'wxl.gen\profiles\base.json') -Raw
if ($base -notmatch '"windowsAppSdk"\s*:\s*"([^"]+)"') { throw 'base.json names no windowsAppSdk release' }
$sdk = $Matches[1]
Get-Package 'Microsoft.WindowsAppSDK' $sdk

$nuspec = [xml](Get-Content (Join-Path $root "microsoft.windowsappsdk\$sdk\microsoft.windowsappsdk.nuspec"))
$declared = @{}
foreach ($d in $nuspec.package.metadata.dependencies.dependency) { $declared[$d.id.ToLowerInvariant()] = $d.version.Trim('[', ']') }
foreach ($id in $manifest.sdkPackages) {
    $version = $declared[$id.ToLowerInvariant()]
    if (-not $version) { throw "Windows App SDK $sdk declares no $id" }
    Get-Package $id $version
}
foreach ($p in $manifest.extra) { Get-Package $p.id $p.version }
foreach ($p in $manifest.experimental) { Get-Package $p.id $p.version }
"done"
