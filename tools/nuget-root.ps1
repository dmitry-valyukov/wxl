# Where the NuGet packages the tree reads live -- dot-sourced by the scripts
# that need to know.
#
# NuGet's own order of preference, so the answer matches what a restore would
# have used: the environment variable, then the configured global folder,
# then the default beside the profile. The C++ side (cmake/nuget_root.cmake and
# winui-srcgen) asks in the same order.
function Resolve-NuGetRoot([string]$Override) {
    if ($Override) { return $Override }
    if ($env:NUGET_PACKAGES) { return $env:NUGET_PACKAGES }
    $config = Join-Path $env:APPDATA 'NuGet\NuGet.Config'
    if (Test-Path $config) {
        $folder = ([xml](Get-Content $config)).configuration.config.add |
            Where-Object { $_.key -eq 'globalPackagesFolder' } |
            Select-Object -First 1 -ExpandProperty value
        if ($folder) { return [Environment]::ExpandEnvironmentVariables($folder) }
    }
    Join-Path $env:USERPROFILE '.nuget\packages'
}
