# Песочница: рецепты, проверенные на живом рантайме WinUI. В samples лежат
# образцы того, как писать на wxl; здесь -- как устроена сама wxl внутри:
# как получить настоящий объект WinUI под своим наследованием и как перехватить
# у него protected-переопределения. Каждый рецепт -- отдельное приложение в
# своей папке, ни с кем ничем не делится.
#
# Папки подхватываются сами: новая проба -- это каталог со своим CMakeLists.txt,
# и этот файл ради неё править не надо. Каталог без CMakeLists.txt в сборку не
# идёт: рецепт, которому в дереве не хватает кода, остаётся текстом, и его
# README.md говорит, чего именно.
file(GLOB _wxl_sandbox_apps CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*/CMakeLists.txt")

foreach(_app IN LISTS _wxl_sandbox_apps)
    cmake_path(GET _app PARENT_PATH _dir)
    add_subdirectory("${_dir}")
endforeach()
