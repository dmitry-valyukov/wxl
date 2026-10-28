# The global packages folder of NuGet, where the packages the tree reads are --
# dot-sourced by the scripts that need to know.
#
# It follows NuGet's own order for the global packages folder: the
# environment variable NUGET_PACKAGES, then globalPackagesFolder of the user's
# NuGet.Config, then the default beside the profile. winui-srcgen asks in the
# same order.
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
