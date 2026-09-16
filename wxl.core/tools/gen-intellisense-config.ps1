<#
.SYNOPSIS
    Генерирует .vscode/c_cpp_properties.json так, чтобы IntelliSense видел
    собранные интерфейсы модулей (.ifc).

.DESCRIPTION
    Расширение C/C++ (cpptools) умеет читать MSVC-шные .ifc, но только если ему
    явно передать /reference <модуль>=<путь к .ifc>. CMake кладёт ровно такие
    ключи в файлы ответов *.obj.modmap рядом с объектниками, однако в
    compile_commands.json попадает только ссылка "@...modmap", которую cpptools
    не разворачивает, — поэтому в редакторе import'ы остаются нерезолвленными.

    Скрипт собирает все *.obj.modmap выбранной конфигурации, разворачивает пути
    в абсолютные и выписывает их в compilerArgs. Перезапускать после того, как
    появились новые .ixx (или после чистой пересборки).

.PARAMETER Config
    Конфигурация сборки, чьи .ifc использовать (Debug по умолчанию).

.PARAMETER BuildDir
    Каталог сборки, по умолчанию build\x64 в корне репозитория.

.PARAMETER IfIde
    Ничего не делать, если сборка запущена не из среды разработки. Нужно для
    post-build шага: в CI и при сборке из консоли обновлять конфиг редактора
    незачем. Признак среды берётся из переменных окружения, которые выставляет
    сама IDE (см. Get-IdeMarker ниже); окружение Developer Command Prompt
    признаком не считается.

.PARAMETER Log
    Дописать строку о принятом решении в указанный файл (используется
    post-build шагом, чтобы решение можно было потом посмотреть).
#>
[CmdletBinding()]
param(
    [string] $Config = 'Debug',
    [string] $BuildDir,
    [switch] $IfIde,
    [string] $Log
)

$ErrorActionPreference = 'Stop'

# Переменные, которые выставляет именно IDE своим дочерним процессам.
# Осознанно НЕ используются VisualStudioVersion/VSINSTALLDIR/VSLANG: их ставит
# и vcvars64.bat, то есть обычная консольная сборка выглядела бы как сборка из
# студии.
function Get-IdeMarker {
    $markers = @(
        @{ Name = 'VS Code (VSCODE_PID)';        Value = $env:VSCODE_PID }
        @{ Name = 'VS Code (VSCODE_CWD)';        Value = $env:VSCODE_CWD }
        @{ Name = 'VS Code (VSCODE_IPC_HOOK)';   Value = $env:VSCODE_IPC_HOOK }
        @{ Name = 'VS Code (VSCODE_IPC_HOOK_CLI)'; Value = $env:VSCODE_IPC_HOOK_CLI }
        @{ Name = 'VS Code (VSCODE_GIT_IPC_HANDLE)'; Value = $env:VSCODE_GIT_IPC_HANDLE }
        @{ Name = 'Visual Studio (VSAPPIDNAME)'; Value = $env:VSAPPIDNAME }
        @{ Name = 'Visual Studio (VSAPPIDDIR)';  Value = $env:VSAPPIDDIR }
    )
    foreach ($m in $markers) {
        if ($m.Value) { return $m.Name }
    }
    if ($env:TERM_PROGRAM -eq 'vscode') { return 'VS Code (TERM_PROGRAM)' }
    return $null
}

$ideMarker = Get-IdeMarker

if ($Log) {
    $verdict = if ($ideMarker) { "IDE: $ideMarker" } else { 'IDE не обнаружена' }
    $line = "[$Config] $verdict"
    Add-Content -Path $Log -Value $line -Encoding utf8
}

if ($IfIde -and -not $ideMarker) {
    Write-Host "Сборка запущена не из IDE — конфигурация IntelliSense не трогается."
    return
}

$root = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) { $BuildDir = Join-Path $root 'build\x64' }
if (-not (Test-Path $BuildDir)) { throw "Каталог сборки не найден: $BuildDir" }

# --- компилятор берём из compile_commands.json, чтобы не разъезжаться со сборкой
$compileCommands = Join-Path $BuildDir 'compile_commands.json'
$compilerPath = $null
if (Test-Path $compileCommands) {
    $first = (Get-Content $compileCommands -Raw | ConvertFrom-Json) | Select-Object -First 1
    if ($first.command -match '^"([^"]+cl\.exe)"') { $compilerPath = $Matches[1] }
}
if (-not $compilerPath) { throw "Не удалось определить путь к cl.exe из $compileCommands" }

# --- собираем отображение "имя модуля -> .ifc" из всех modmap'ов конфигурации
$modules = [ordered]@{}
$modmaps = Get-ChildItem $BuildDir -Recurse -Filter '*.obj.modmap' |
    Where-Object { $_.FullName -like "*\$Config\*" }

foreach ($modmap in $modmaps) {
    foreach ($line in Get-Content $modmap.FullName) {
        if ($line -notmatch '^-reference\s+"([^=]+)=(.+)"$') { continue }
        $name = $Matches[1]
        $ifc = [System.IO.Path]::GetFullPath((Join-Path $BuildDir $Matches[2]))
        if (Test-Path $ifc) { $modules[$name] = $ifc.Replace('\', '/') }
    }
}
if ($modules.Count -eq 0) {
    throw "В $BuildDir не найдено ни одного modmap для конфигурации $Config — сначала соберите проект."
}

# --- ключи компилятора: то же, что в compile_commands, плюс ссылки на модули
$compilerArgs = [System.Collections.Generic.List[string]]::new()
'/std:c++latest', '/EHsc', '/permissive-', '/utf-8' | ForEach-Object { $compilerArgs.Add($_) }
if ($Config -eq 'Debug') { $compilerArgs.Add('/MDd') } else { $compilerArgs.Add('/MD') }
foreach ($name in $modules.Keys) {
    $compilerArgs.Add('/reference')
    $compilerArgs.Add("$name=$($modules[$name])")
}

$defines = @(
    'WXL_VERSION="0.1.0"'
    '_MBCS'
    'WIN32'
    '_WINDOWS'
    "CMAKE_INTDIR=`"$Config`""
)
if ($Config -ne 'Debug') { $defines += 'NDEBUG' }

$configuration = [ordered]@{
    name              = 'wxl.core (modules)'
    includePath       = @('${workspaceFolder}/src')
    defines           = $defines
    compilerPath      = $compilerPath.Replace('\', '/')
    compilerArgs      = $compilerArgs
    cStandard         = 'c17'
    cppStandard       = 'c++23'
    intelliSenseMode  = 'windows-msvc-x64'
}

# принудительный include PCH — как в реальной сборке (/FI cmake_pch.hxx)
$pch = Join-Path $BuildDir "src\CMakeFiles\wxl.core.dir\$Config\cmake_pch.hxx"
if (Test-Path $pch) { $configuration.forcedInclude = @($pch.Replace('\', '/')) }

$json = [ordered]@{
    configurations = @($configuration)
    version        = 4
} | ConvertTo-Json -Depth 6

$outDir = Join-Path $root '.vscode'
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }
$outFile = Join-Path $outDir 'c_cpp_properties.json'
Set-Content -Path $outFile -Value $json -Encoding utf8

Write-Host "Записано: $outFile"
Write-Host "Конфигурация: $Config, модулей подключено: $($modules.Count)"
