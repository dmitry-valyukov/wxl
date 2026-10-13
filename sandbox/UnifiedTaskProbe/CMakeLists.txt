# Проба единого task<R>: что стоят горячие пути wxl.async -- операция у воркера, корутина,
# цепочки подпрограмм до операции, то же под токеном -- и держит ли цепочка любой глубины одну
# схему. Консольная, как бенчмарки wxl.async: только wxl::async, без окна -- wxl::ui увёл бы вход
# на свой wWinMain. Код возврата -- число провалившихся проверок.
add_executable(sandbox.unified-task-probe main.cpp)

target_link_libraries(sandbox.unified-task-probe PRIVATE wxl::async)

wxl_set_target_folders(sandbox.unified-task-probe)
