wxl_add_module_library(wxl.rsdn STATIC
    PUBLIC
        wxl.rsdn.ixx
    SRC
        rsdn.cpp
)

add_library(wxl::rsdn ALIAS wxl.rsdn)

# PUBLIC: parse() отвечает html::document — дерево семейства стоит в
# интерфейсе, и с ним приезжают wxl::core и wxl::xml.
target_link_libraries(wxl.rsdn PUBLIC wxl::html)

# PRIVATE: подсветка кода живёт внутри токенизатора -- наружу выходит
# дерево семейства со <span style="kw">, а не куски wxl.highlight.
target_link_libraries(wxl.rsdn PRIVATE wxl::highlight)

# Как в wxl.html: исключением может ответить только аллокация.
if(MSVC AND NOT CMAKE_CXX_FLAGS MATCHES "/EHsc")
    target_compile_options(wxl.rsdn PUBLIC /EHsc)
endif()

wxl_set_target_folders(wxl.rsdn)
