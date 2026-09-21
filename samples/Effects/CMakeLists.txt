# Витрина эффектов: по эффекту на .cpp, общая раскладка страницы в
# Showcase.cpp, окно со списком слева в main.cpp.

# Сниппеты: код примеров, который и компилируется (#include "X.h"), и
# показывается (#include "X.h.embed" — байты того же файла, см. embed.cmake),
# и описания страниц (page.html, только показываются).
# В списке исходников — чтобы IDE видела их рядом со страницами.
file(GLOB_RECURSE _effects_snippets CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/Snippets/*.h"
     "${CMAKE_CURRENT_SOURCE_DIR}/Snippets/*.html")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Snippets" PREFIX "Snippets"
             FILES ${_effects_snippets})

set(_effects_embedded "")
foreach(_snippet IN LISTS _effects_snippets)
    file(RELATIVE_PATH _relative "${CMAKE_CURRENT_SOURCE_DIR}" "${_snippet}")
    set(_embedded "${CMAKE_CURRENT_BINARY_DIR}/${_relative}.embed")
    add_custom_command(
        OUTPUT "${_embedded}"
        COMMAND "${CMAKE_COMMAND}" "-DIN=${_snippet}" "-DOUT=${_embedded}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/embed.cmake"
        DEPENDS "${_snippet}" "${CMAKE_CURRENT_SOURCE_DIR}/embed.cmake"
        COMMENT "Сниппет ${_relative}.embed"
        VERBATIM)
    list(APPEND _effects_embedded "${_embedded}")
endforeach()

wxl_add_executable(sample.effects
    Pages.h
    pch.h
    main.cpp
    Showcase.cpp
    HaloPage.cpp
    MagnifyPage.cpp
    BevelPage.cpp
    ${_effects_snippets}
    ${_effects_embedded}
    embed.cmake
)

# Байты сниппетов лежат в дереве сборки под тем же путём, что и сами сниппеты.
target_include_directories(sample.effects PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

# Шрифты и картинки образца -- его собственные, из его Assets, рядом с exe.
wxl_target_assets(sample.effects Assets)
