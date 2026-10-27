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
#  - the pinned "extra" packages of tools/packages.json.
# A package already in the folder is left alone, so a second run is a no-op.
# The layout is NuGet's own (<root>\<id lower case>\<version>\), which is what
# every other script and the generator read.
[CmdletBinding()]
param([string]$NuGetRoot)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'nuget-root.ps1')
. (Join-Path $PSScriptRoot 'windows-app-sdk.ps1')
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
    # nuget.org occasionally terminates a response before the file is complete
    # ("The response ended prematurely"). Every CI job runs this step, so a single
    # truncated response would fail the job before the build. Up to three attempts
    # with increasing pauses; each attempt downloads the file anew after removing
    # the partial one.
    $pauses = 5, 15
    for ($attempt = 1; ; $attempt++) {
        try {
            Invoke-WebRequest -Uri $url -OutFile $file -UseBasicParsing
            break
        } catch {
            Remove-Item $file -ErrorAction SilentlyContinue
            if ($attempt -gt $pauses.Count) { throw "$id $version not fetched from $url after $attempt attempts: $($_.Exception.Message)" }
            "retry   $id $version in $($pauses[$attempt - 1]) s: $($_.Exception.Message)"
            Start-Sleep -Seconds $pauses[$attempt - 1]
        }
    }
    try {
        New-Item -ItemType Directory -Force $dir | Out-Null
        Expand-Archive -LiteralPath $file -DestinationPath $dir -Force
    } finally {
        Remove-Item $file -ErrorAction SilentlyContinue
    }
    if (-not (Test-Path (Join-Path $dir "$low.nuspec"))) { throw "$id $version unpacked without a nuspec into $dir" }
}

"package folder: $root"

$sdk = Get-WindowsAppSdkRelease $repo
Get-Package 'Microsoft.WindowsAppSDK' $sdk

$declared = Get-WindowsAppSdkPackageVersion $root $sdk
foreach ($id in $manifest.sdkPackages) {
    $version = $declared[$id.ToLowerInvariant()]
    if (-not $version) { throw "Windows App SDK $sdk declares no $id" }
    Get-Package $id $version
}
foreach ($p in $manifest.extra) { Get-Package $p.id $p.version }
"done"
