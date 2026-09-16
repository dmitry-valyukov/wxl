wxl_add_module_library(wxl.highlight STATIC
    PUBLIC
        wxl.highlight.ixx
    SRC
        highlight.cpp
        languages.cpp
)

add_library(wxl::highlight ALIAS wxl.highlight)

# Как у соседей: исключением здесь не отвечает никто, но модульный std
# собирается с раскруткой, и один набор ключей на всё дерево дешевле.
if(MSVC AND NOT CMAKE_CXX_FLAGS MATCHES "/EHsc")
    target_compile_options(wxl.highlight PUBLIC /EHsc)
endif()

wxl_set_target_folders(wxl.highlight)
