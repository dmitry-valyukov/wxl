# /cmake/wxl_target_folders.cmake
#
# Recreates the on-disk folder structure inside IDEs (Visual Studio and other
# generators that honour FOLDER / source_group).

include_guard(GLOBAL)

macro(_wxl_append_if_valid _dts _srs)
    if(${_srs})
        list(APPEND ${_dts} ${${_srs}})
    endif()
endmacro()

function(_wxl_replace_prefix path_var prefix)
    set(_current_path "${${path_var}}")

    # Пограничный случай 1: Пустой путь
    if(_current_path STREQUAL "")
        set(${path_var} "${prefix}" PARENT_SCOPE)
        return()
    endif()

    # Превращаем путь в список CMake, заменяя слэши на точки с запятой
    string(REPLACE "/" ";" _components "${_current_path}")

    # Удаляем самый первый элемент списка (первый компонент пути)
    list(REMOVE_AT _components 0)

    # Пограничный случай 2: Путь состоял всего из 1 компонента (список теперь пуст)
    if(_components STREQUAL "")
        set(${path_var} "${prefix}" PARENT_SCOPE)
    else()
        # Собираем хвост обратно через слэш
        string(REPLACE ";" "/" _tail "${_components}")

        # Соединяем новый префикс и оставшийся хвост
        set(${path_var} "${prefix}/${_tail}" PARENT_SCOPE)
    endif()
endfunction()


function(wxl_set_target_folders target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR
            "\n[ERROR]: Target '${target}' does not exist!\n"
            "Make sure you call wxl_set_target_folders() strictly AFTER defining the target via add_executable() or add_library().\n"
        )
    endif()

    set(oneValueArgs PREFIX)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    get_target_property(_target_dir ${target} SOURCE_DIR)

    cmake_path(RELATIVE_PATH _target_dir BASE_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE _rel_path)
    cmake_path(NORMAL_PATH _rel_path)
    cmake_path(GET _rel_path PARENT_PATH _folder)

    if(DEFINED ARG_PREFIX)
        _wxl_replace_prefix(_folder "${ARG_PREFIX}")
    endif()

    # Устанавливаем путь (если там пустая строка "", CMake корректно положит таргет в корень)
    set_target_properties(${target} PROPERTIES FOLDER "${_folder}")

    set(_all_files "")

    # --- ЧАСТЬ 1: Получаем классические файлы (.cpp, .h) ---
    get_target_property(_files ${target} SOURCES)
    _wxl_append_if_valid(_all_files _files)

    # --- ЧАСТЬ 2: Получаем C++20/26 модули (.cppm) ---
    get_target_property(_module_sets ${target} CXX_MODULE_SETS)

    if(_module_sets)
        foreach(_module_set IN LISTS _module_sets)
            get_target_property(_files ${target} CXX_MODULE_SET_${_module_set})
            _wxl_append_if_valid(_all_files _files)

            get_target_property(_files ${target} INTERFACE_CXX_MODULE_SET_${_module_set})
            _wxl_append_if_valid(_all_files _files)
        endforeach()
    endif()

    # Очистка списка от мусора
    list(REMOVE_ITEM _all_files "" NOTFOUND)
    list(REMOVE_DUPLICATES _all_files)

    # --- ЧАСТЬ 3: Группировка файлов по папкам диска (Магия TREE) ---
    set(_abs_files "")
    foreach(_file IN LISTS _all_files)
        cmake_path(ABSOLUTE_PATH _file BASE_DIRECTORY "${_target_dir}" NORMALIZE OUTPUT_VARIABLE _abs_file)
        if(EXISTS "${_abs_file}")
            list(APPEND _abs_files "${_abs_file}")
        endif()
    endforeach()

    # СБРОС ДЕФОЛТНЫХ ПАПОК VISUAL STUDIO:
    # Принудительно очищаем стандартные группы "Source Files" и "Header Files" для этого таргета
    source_group("Source Files" REGEX "^$")
    source_group("Header Files" REGEX "^$")

    # Идеально строит дерево исходников в IDE напрямую от корня проекта
    source_group(TREE "${_target_dir}" FILES ${_abs_files})


    # --- ЧАСТЬ 4: Перехват .cmake файлов с сохранением структуры папок ---
    get_directory_property(_other_depends CMAKE_CONFIGURE_DEPENDS)

    set(_cmake_scripts "")
    foreach(_dep IN LISTS _other_depends)
        cmake_path(GET _dep EXTENSION _ext)
        if(_ext STREQUAL ".cmake")
            # Приводим к абсолютному виду для корректной работы TREE
            cmake_path(ABSOLUTE_PATH _dep BASE_DIRECTORY "${CMAKE_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE _abs_dep)
            if(EXISTS "${_abs_dep}")
                list(APPEND _cmake_scripts "${_abs_dep}")
            endif()
        endif()
    endforeach()

    # Если скрипты найдены, раскладываем их по дереву папок относительно CMAKE_SOURCE_DIR
    # Визуальным корнем в IDE для них станет папка "CMake Scripts"
    if(_cmake_scripts)
        source_group(TREE "${CMAKE_SOURCE_DIR}" PREFIX "CMake Scripts" FILES ${_cmake_scripts})
    endif()

endfunction()

# Применяет группировку ко всем целям ОДНОГО каталога -- того, из которого
# вызвана. Не всего дерева: BUILDSYSTEM_TARGETS перечисляет цели текущего
# каталога и не спускается в подкаталоги, а чтобы обойти дерево, пришлось бы
# рекурсивно идти по свойству SUBDIRECTORIES.
#
# В самой wxl её никто не зовёт, и заменить ею поимённые вызовы нельзя: часть
# целей просит PREFIX (`tests`), а здесь его передать некому.
function(wxl_group_all_targets)
    get_directory_property(_all_targets BUILDSYSTEM_TARGETS)

    set(_excluded_target_types "INTERFACE_LIBRARY" "UTILITY")

    foreach(_tgt IN LISTS _all_targets)
        # Проверяем тип таргета (игнорируем служебные интерфейсные библиотеки без файлов)
        get_target_property(_tgt_type ${_tgt} TYPE)
        if(NOT _tgt_type IN_LIST _excluded_target_types)
            wxl_set_target_folders(${_tgt})
        endif()
    endforeach()
endfunction()
