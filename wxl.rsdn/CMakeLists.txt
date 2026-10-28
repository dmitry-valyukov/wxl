# ---- CMakeList.txt : wxl.rsdn ----
#
# Разметка RSDN поверх дерева wxl.html: свой токенизатор, общий
# tree_builder семейства.

# Политики этого каталога -- CMake 3.28, а не корня: более поздние меняют то,
# что компилируют его цели (CMP0204, например, определяет _MBCS в каждой цели,
# созданной под ним).
cmake_minimum_required(VERSION 3.28)

add_subdirectory(src)

if(WXL_TESTS)
    add_subdirectory(tests)
endif()
