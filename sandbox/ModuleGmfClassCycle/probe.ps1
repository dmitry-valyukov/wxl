# Прогон пробы: каждый вариант заголовка компилируется заново — модуль,
# интерфейс потребителя, обычная единица трансляции, программа, — и на вариант
# печатается строка с исходом каждого шага. Ошибка тут — отказ компилятора, а
# не поведение программы, поэтому проба не собирается CMake вместе с деревом:
# её ведёт этот скрипт, зовя cl напрямую.
#
#   pwsh sandbox\ModuleGmfClassCycle\probe.ps1                 # установленный тулсет
#   pwsh sandbox\ModuleGmfClassCycle\probe.ps1 -Toolset Preview

[CmdletBinding()]
param(
    # Передаётся в vcvars64.bat как -vcvars_ver; пусто — тулсет по умолчанию.
    [string]$Toolset,

    # Каталог с настоящим winmd_reader.h, для сверки уменьшенной формы с ним;
    # по умолчанию — то, что скачала сборка дерева.
    [string]$WinmdInclude = (Join-Path $PSScriptRoot '..\..\build\x64\_deps\winmd-src\src')
)

$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$install = & $vswhere -latest -prerelease -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $install 'VC\Auxiliary\Build\vcvars64.bat'
$version = if ($Toolset) { "-vcvars_ver=$Toolset" } else { '' }

& "$env:ComSpec" /c "`"$vcvars`" $version >nul 2>nul && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
}

$banner = (& cl 2>&1 | Where-Object { $_ -match 'Version' } | Select-Object -First 1)
Write-Output "toolset $env:VCToolsVersion; $banner"

$flags = @('/nologo', '/std:c++latest', '/EHsc', '/permissive-', '/utf-8', '/MDd', '/Od', '/Zi')

# Форма заголовка (reader.h), затем обходы при нетронутом заголовке
# (common.ixx, consumer.ixx, consumer_impl.cpp).
$variants = [ordered]@{
    'winmd'           = @()
    'no-forward'      = @('/DPROBE_NO_FORWARD')
    'no-crtp'         = @('/DPROBE_NO_CRTP')
    'explicit-return' = @('/DPROBE_EXPLICIT_RETURN')
    'gmf-call'        = @('/DPROBE_GMF_CALL')
    'purview-call'    = @('/DPROBE_PURVIEW_CALL')
    'purview-type'    = @('/DPROBE_PURVIEW_TYPE')
    'purview-complete' = @('/DPROBE_PURVIEW_COMPLETE')
    'enum-first'      = @('/DPROBE_ENUM_FIRST')
}

function Step([string]$out, [string[]]$arguments) {
    $log = & cl @flags @arguments 2>&1
    if ($LASTEXITCODE -eq 0) { return 'OK' }
    $first = "$($log | Where-Object { $_ -match 'error' } | Select-Object -First 1)"
    $message = $first -replace '^.*?\): ', ''
    return "FAIL ($message)"
}

foreach ($name in $variants.Keys) {
    $out = Join-Path $here "out\$(if ($Toolset) { $Toolset } else { 'default' })\$name"
    New-Item -ItemType Directory -Force $out | Out-Null
    $defines = $variants[$name]
    $common = @('/ifcSearchDir', $out, '/ifcOutput', "$out\", "/Fo$out\", "/Fd$out\")

    $module = Step $out ($defines + $common + @('/c', '/interface', "$here\common.ixx"))
    $interface = Step $out ($defines + $common + @('/c', '/interface', "$here\consumer.ixx"))
    $plain = Step $out ($defines + $common + @('/c', "$here\consumer_impl.cpp"))

    $run = '-'
    if ($interface -eq 'OK') {
        $built = Step $out ($defines + $common + @("/Fe$out\probe.exe", "$here\main.cpp",
                                                   "$out\common.obj", "$out\consumer.obj"))
        $run = if ($built -eq 'OK') { "prints $(& "$out\probe.exe")" } else { "build $built" }
    }

    Write-Output ("{0,-16} module {1}; interface {2}; plain TU {3}; run {4}" -f $name, $module, $interface, $plain, $run)
}

# Контроль: заголовок и потребитель — партиции одного модуля.
$controls = [ordered]@{
    'same-module'     = @()
    'same-complete'   = @('/DPROBE_PURVIEW_COMPLETE')
    'same-gmf-call'   = @('/DPROBE_GMF_CALL')
}

foreach ($name in $controls.Keys) {
    $out = Join-Path $here "out\$(if ($Toolset) { $Toolset } else { 'default' })\$name"
    New-Item -ItemType Directory -Force $out | Out-Null
    $defines = $controls[$name]
    $common = @('/ifcSearchDir', $out, '/ifcOutput', "$out\", "/Fo$out\", "/Fd$out\")
    $reader = Step $out ($defines + $common + @('/c', '/interface', "$here\same_reader.ixx"))
    $facts = Step $out ($defines + $common + @('/c', '/interface', "$here\same_facts.ixx"))
    $primary = Step $out ($defines + $common + @('/c', '/interface', "$here\same.ixx"))
    Write-Output ("{0,-16} :reader {1}; :facts {2}; primary {3}" -f $name, $reader, $facts, $primary)
}

# Настоящий winmd_reader.h (microsoft/winmd), если он есть под рукой: те же две
# раскладки, чтобы сверить уменьшенную форму с оригиналом.
if ($WinmdInclude -and (Test-Path (Join-Path $WinmdInclude 'winmd_reader.h'))) {
    $out = Join-Path $here "out\$(if ($Toolset) { $Toolset } else { 'default' })\real"
    New-Item -ItemType Directory -Force $out | Out-Null
    $common = @("/I$WinmdInclude", '/ifcSearchDir', $out, '/ifcOutput', "$out\", "/Fo$out\", "/Fd$out\")

    $module = Step $out ($common + @('/c', '/interface', "$here\real_winmd.ixx"))
    $interface = Step $out ($common + @('/c', '/interface', "$here\real_consumer.ixx"))
    Write-Output ("{0,-16} module {1}; interface {2}" -f 'real', $module, $interface)

    $reader = Step $out ($common + @('/c', '/interface', "$here\real_same_reader.ixx"))
    $facts = Step $out ($common + @('/c', '/interface', "$here\real_same_facts.ixx"))
    $primary = Step $out ($common + @('/c', '/interface', "$here\real_same.ixx"))
    Write-Output ("{0,-16} :reader {1}; :facts {2}; primary {3}" -f 'real-same', $reader, $facts, $primary)
}
