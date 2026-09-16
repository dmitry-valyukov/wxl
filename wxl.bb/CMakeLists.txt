# ---- CMakeList.txt : wxl.bb ----
#
# BB-код форумов поверх дерева wxl.html: свой токенизатор, общий
# tree_builder семейства. Устройство сборки — как у wxl.html рядом.
cmake_minimum_required(VERSION 3.28)

set(WXL_CMAKE_SCRIPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    include(${WXL_CMAKE_SCRIPT_DIR}/vcpkg_bootstrap.cmake)
    include(${WXL_CMAKE_SCRIPT_DIR}/wxl_import_std_gate.cmake)

    project(
        wxl-bb VERSION 0.1.0
        DESCRIPTION "Winui Xaml-Less library (wxl) -- BB markup"
        LANGUAGES CXX
    )

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_CXX_MODULE_STD 1)
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)

    option(BUILD_TESTING "Build the testing tree." ON)

    if(BUILD_TESTING)
        enable_testing()
        find_package(GTest REQUIRED CONFIG)
        include(GoogleTest)
    endif()

    include(${WXL_CMAKE_SCRIPT_DIR}/config.cmake)
endif()

if(NOT TARGET wxl::core)
    find_package(wxl CONFIG REQUIRED COMPONENTS core)
endif()

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
