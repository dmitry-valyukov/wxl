wxl_add_module_library(wxl.bb STATIC
    PUBLIC
        wxl.bb.ixx
    SRC
        bb.cpp
)

add_library(wxl::bb ALIAS wxl.bb)

# PUBLIC: parse() отвечает html::document — дерево семейства стоит в
# интерфейсе, и с ним приезжают wxl::core и wxl::xml.
target_link_libraries(wxl.bb PUBLIC wxl::html)

# Как в wxl.html: исключением может ответить только аллокация.
if(MSVC AND NOT CMAKE_CXX_FLAGS MATCHES "/EHsc")
    target_compile_options(wxl.bb PUBLIC /EHsc)
endif()

wxl_set_target_folders(wxl.bb)
