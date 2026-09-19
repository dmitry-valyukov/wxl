# The sample application -- an application as one written against wxl looks:
# it links wxl::ui and nothing else, and it never sees the cppwinrt
# projection, because reaching wxl's declarative surface must not cost a
# consumer the projection's parse time.
#
# wxl_add_executable() is the whole of that: a GUI subsystem executable, so
# that wxl's own wWinMain is the entry point, linked against wxl::ui, with
# what the library needs at run time beside it.

wxl_add_executable(sample.calc
    Calc.h
    Calc.cpp
    main.cpp
)

# An image path carrying no scheme is resolved next to the executable, so the
# assets have to be there. Each file is an output of the build with a
# dependency of its own, so editing one copies it and nothing else.
wxl_target_assets(sample.calc Assets)
