<#
.SYNOPSIS
    Собирает NuGet-пакет wxl.core из уже собранного дерева CMake.

.DESCRIPTION
    Пакет — это тот же install-префикс CMake (include\wxl, lib\*.lib,
    lib\cmake\wxl) плюс build\native\*.props|targets для MSBuild. Один
    артефакт обслуживает оба мира: MSBuild-проект просто ставит зависимость,
    CMake-проект указывает CMAKE_PREFIX_PATH на распакованный пакет.

    Собранные .ifc в пакет не кладутся принципиально: они привязаны к
    конкретной сборке MSVC, поэтому интерфейсы модулей компилируются у
    потребителя. Версия тулсета, которым собрана .lib, записывается в props и
    проверяется в targets.

.PARAMETER BuildDir
    Каталог сборки CMake, по умолчанию build\x64 в корне репозитория.

.PARAMETER Version
    Версия пакета; по умолчанию берётся из project(... VERSION ...).

.PARAMETER Output
    Куда положить .nupkg (по умолчанию build\nuget).
#>
[CmdletBinding()]
param(
    [string] $BuildDir,
    [string] $Version,
    [string] $Output
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not $BuildDir) { $BuildDir = Join-Path $root 'build\x64' }
if (-not $Output) { $Output = Join-Path $root 'build\nuget' }
if (-not (Test-Path $BuildDir)) { throw "Каталог сборки не найден: $BuildDir" }

# --- версия и тулсет берём из самой сборки, чтобы не разъезжаться с ней
if (-not $Version) {
    $cmakeLists = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw
    if ($cmakeLists -match 'project\s*\(\s*\S+\s+VERSION\s+([0-9.]+)') { $Version = $Matches[1] }
    else { throw 'Не удалось определить версию из project(... VERSION ...)' }
}

$compileCommands = Join-Path $BuildDir 'compile_commands.json'
if (-not (Test-Path $compileCommands)) { throw "Нет $compileCommands — сначала сконфигурируйте проект" }
$firstCommand = ((Get-Content $compileCommands -Raw | ConvertFrom-Json) | Select-Object -First 1).command
if ($firstCommand -notmatch 'MSVC[\\/]([0-9.]+)[\\/]bin') { throw 'Не удалось определить версию MSVC из compile_commands.json' }
$vcToolsVersion = $Matches[1]

# --- staging: install-префикс CMake + подставленные props
$stage = Join-Path $BuildDir 'nuget-stage'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null

foreach ($config in @('Debug', 'Release')) {
    Write-Host "Установка конфигурации $config в staging..."
    & cmake --install $BuildDir --config $config --prefix $stage | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "cmake --install ($config) завершился с кодом $LASTEXITCODE" }
}

$buildNativeSrc = Join-Path $PSScriptRoot 'build\native'
$packBase = Join-Path $BuildDir 'nuget-base'          # -BasePath для nuget pack
$buildNativeDst = Join-Path $packBase 'build\native'  # путь, который ждёт nuspec
if (Test-Path $packBase) { Remove-Item $packBase -Recurse -Force }
New-Item -ItemType Directory -Path $buildNativeDst -Force | Out-Null

foreach ($file in Get-ChildItem $buildNativeSrc -File) {
    $text = Get-Content $file.FullName -Raw
    $text = $text.Replace('@WXL_VCTOOLS_VERSION@', $vcToolsVersion).Replace('@WXL_VERSION@', $Version)
    Set-Content -Path (Join-Path $buildNativeDst $file.Name) -Value $text -Encoding utf8
}

# --- пакуем
if (-not (Test-Path $Output)) { New-Item -ItemType Directory -Path $Output | Out-Null }

$nuget = (Get-Command nuget -ErrorAction SilentlyContinue).Source
if (-not $nuget) { throw 'nuget.exe не найден в PATH' }

$nuspec = Join-Path $PSScriptRoot 'wxl.core.nuspec'
& $nuget pack $nuspec `
    -BasePath $packBase `
    -Properties "version=$Version;stage=$stage" `
    -OutputDirectory $Output `
    -NoDefaultExcludes | Write-Host
if ($LASTEXITCODE -ne 0) { throw "nuget pack завершился с кодом $LASTEXITCODE" }

Write-Host ""
Write-Host "Пакет: $Output\wxl.core.$Version.nupkg"
Write-Host "MSVC:  $vcToolsVersion (записан в props для проверки у потребителя)"
