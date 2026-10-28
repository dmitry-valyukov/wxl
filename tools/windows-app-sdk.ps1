# The Windows App SDK release the tree builds against and the versions of the
# packages it ships -- dot-sourced by the scripts that need them.
#
# The release is the one "windowsAppSdk" names in wxl.gen/profiles/base.json.
# Its nuspec has one <dependency> per package that comes with it. NuGet reads a
# plain version there as a minimum and only a bracketed one ([1.2.3]) as exact;
# these functions take each declared version as the version of the package,
# which is the one the restore of the tree resolves while no other reference
# asks for a higher one. They read only <dependency> elements directly under
# <dependencies>, each with a plain version or a one-version range; winui-srcgen
# (wxl.gen.common/xml_input.cpp, profile.cpp) also reads dependencies grouped by
# target framework and the lower bound of a wider range. The nuspec is in the
# NuGet package folder after a configure of the tree, whose restore
# (cmake/wxl_packages.cmake) puts the release there.

# The release named by "windowsAppSdk" in wxl.gen/profiles/base.json.
function Get-WindowsAppSdkRelease([string]$Repo) {
    $base = Get-Content (Join-Path $Repo 'wxl.gen\profiles\base.json') -Raw
    if ($base -notmatch '"windowsAppSdk"\s*:\s*"([^"]+)"') { throw 'base.json names no windowsAppSdk release' }
    $Matches[1]
}

# The version of every package the release declares, by lower-case id. A
# dependency pinned as a one-version range ([1.2.3]) is read as that version.
function Get-WindowsAppSdkPackageVersion([string]$Root, [string]$Release) {
    $file = Join-Path $Root "microsoft.windowsappsdk\$Release\microsoft.windowsappsdk.nuspec"
    if (-not (Test-Path $file)) { throw "Windows App SDK $Release is not in $Root; configure the tree first: its restore puts the release there" }
    $declared = @{}
    foreach ($d in ([xml](Get-Content $file)).package.metadata.dependencies.dependency) {
        $declared[$d.id.ToLowerInvariant()] = $d.version.Trim('[', ']')
    }
    $declared
}
