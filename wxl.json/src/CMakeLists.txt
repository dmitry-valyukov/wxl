wxl_add_module_library(wxl.json STATIC
    PUBLIC
        wxl.json.ixx

        exception.ixx
        value.ixx
        document.ixx
    SRC
        exception.cpp
        document.cpp
)

add_library(wxl::json ALIAS wxl.json)

# PUBLIC: первичный интерфейс переэкспортирует wxl.core через арену, и
# потребитель компилирует этот переэкспорт у себя.
target_link_libraries(wxl.json PUBLIC wxl::core)

# Арена, из которой вырезано дерево, -- своя схема хранения wxl.xml, взятая
# зависимостью, а не скопированная. PUBLIC потому, что document держит арену
# полем, и заголовок модуля виден потребителю.
target_link_libraries(wxl.json PUBLIC wxl::xml)

# Негодный документ -- это бросок, а MSVC без /EHsc не разматывает ничего:
# деструкторы всего, что лежит на стеке между throw и catch, просто не
# зовутся. Добавляется только когда файл тулчейна заменил флаги целиком, --
# так же, как в wxl.xml.
if(MSVC AND NOT CMAKE_CXX_FLAGS MATCHES "/EHsc")
    target_compile_options(wxl.json PUBLIC /EHsc)
endif()

wxl_set_target_folders(wxl.json)
