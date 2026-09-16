# WXL - WinUI Xaml-Less

Инфраструктура разработки приложений Windows с использованием WinUI 3 без XAML.

## Сборка

Проект использует [vcpkg](https://github.com/microsoft/vcpkg) для установки
зависимостей и `CMakePresets.json` для конфигурирования.

### Требования

1. **Visual Studio с тулчейном, который умеет `import std;`** — на сегодня это
   Visual Studio Insiders. Вся библиотека собрана из модулей C++20, а модульная
   стандартная библиотека требует и свежего тулсета, и генератора Ninja:
   генераторы Visual Studio не строят BMI импортированных целей. Оттуда же
   берутся сами `ninja` и `cmake` — нужен CMake от 4.3 до 4.4, потому что
   экспериментальный ключ `import std;` меняет UUID с каждым выпуском (см.
   `cmake/wxl_import_std_gate.cmake`).

2. Установленный [vcpkg](https://github.com/microsoft/vcpkg), хотя бы раз
   запущенный: сборка находит его через `vcpkg.path.txt`, который vcpkg пишет
   сам. Если этого файла нет, корень можно указать кэш-переменной при
   конфигурировании: `cmake --preset x64 -DVCPKG_ROOT=M:\vcpkg`. Переменная
   среды `VCPKG_ROOT` на Windows сознательно не читается — vcvars64.bat
   подменяет её на vcpkg из состава Visual Studio, в котором нет нужных портов
   (см. `cmake/vcpkg_bootstrap.cmake`).

3. Установить зависимости для триплета `x64-windows-static-md`:

   ```powershell
   %VCPKG_ROOT%\vcpkg.exe install cppwinrt:x64-windows-static-md
   %VCPKG_ROOT%\vcpkg.exe install fmt:x64-windows-static-md
   ```

4. GTest — только если собираешь с тестами. `BUILD_TESTING` выключена по
   умолчанию, и пресеты её не трогают, так что обычная настройка GTest не
   ищет вовсе:

   ```powershell
   %VCPKG_ROOT%\vcpkg.exe install gtest:x64-windows-static-md
   ```

5. Сеть на первую настройку дерева: `winmd` (читатель метаданных WinRT)
   приезжает через `FetchContent`, а сайту документации нужны Doxygen и тема.
   Сайт можно и не собирать — `-DWXL_BUILD_DOCS=OFF`.

### Конфигурирование и сборка

Из обычной оболочки — скриптом:

```powershell
tools\build.ps1                      # настроить (если нужно) и собрать Debug
tools\build.ps1 -Config Release
tools\build.ps1 -Target winui-srcgen # одну цель
tools\build.ps1 -Configure           # заново настроить перед сборкой
```

Скрипт нужен ровно за одним: он сам поднимает окружение Visual Studio —
находит установку через `vswhere` (`-prerelease`, иначе Insiders не
засчитается) и переносит в текущий процесс то, что выставляет `vcvars64.bat`.
Без этого окружения на PATH нет ни `cl.exe`, ни `ninja`, ни подходящего
`cmake`, и пресеты не срабатывают.

Те же пресеты голыми командами работают только внутри Developer PowerShell
(«x64 Native Tools»), где окружение уже поднято:

```powershell
cmake --preset x64
cmake --build --preset x64-debug
```

### Тесты

Дерево настраивается без них, поэтому тесты сперва надо попросить. Значение
остаётся в `CMakeCache.txt`, так что просят один раз на каталог сборки:

```powershell
tools\build.ps1 -Define BUILD_TESTING=ON   # или cmake --preset x64 -DBUILD_TESTING=ON
ctest --preset x64-debug
```

Если `ctest` отвечает «No tests were found», каталог сборки настроен без
тестов — это та самая просьба, которую забыли.

Подробнее [рядом](README2.md)
