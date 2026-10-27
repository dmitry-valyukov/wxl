# The Windows App SDK release the tree builds against and the versions of the
# packages it ships -- dot-sourced by the scripts that need them.
#
# The release is the one "windowsAppSdk" names in wxl.gen/profiles/base.json.
# Its nuspec has one <dependency> per package that comes with it. NuGet reads a
# plain version there as a minimum and only a bracketed one ([1.2.3]) as exact;
# the tree takes each declared version as its pin, which keeps the packages the
# set the release names. These functions read only <dependency> elements
# directly under <dependencies>, each with a plain version or a one-version
# range, and cmake/wxl_packages.cmake reads the same two forms of the version;
# winui-srcgen (wxl.gen.common/xml_input.cpp, profile.cpp) also reads
# dependencies grouped by target framework and the lower bound of a wider range.

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
    if (-not (Test-Path $file)) { throw "Windows App SDK $Release is not in $Root; run tools\restore-packages.ps1" }
    $declared = @{}
    foreach ($d in ([xml](Get-Content $file)).package.metadata.dependencies.dependency) {
        $declared[$d.id.ToLowerInvariant()] = $d.version.Trim('[', ']')
    }
    $declared
}
