# Проба единого task<R>: что стоят горячие пути async в wxl.core -- операция у воркера, корутина,
# цепочки подпрограмм до операции, то же под токеном -- и держит ли цепочка любой глубины одну
# схему. Консольная, как бенчмарки async: только wxl::core, без окна -- wxl::ui увёл бы вход
# на свой wWinMain. Код возврата -- число провалившихся проверок.
add_executable(sandbox.unified-task-probe main.cpp)

target_link_libraries(sandbox.unified-task-probe PRIVATE wxl::core)

# Манифест исполняемых файлов wxl, как у всякой программы дерева на wxl.
wxl_target_manifest(sandbox.unified-task-probe)
wxl_set_target_folders(sandbox.unified-task-probe)
