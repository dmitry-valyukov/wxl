# The window of wxl's own with a title bar of its own, written in braces: the
# title bar built right where the window is described, the caption buttons
# drawn by the window at the height of that bar, and the whole island zoomed
# with the keys a browser uses. Like every sample, it links wxl::ui and never
# sees the cppwinrt projection.

wxl_add_executable(sample.custom-titlebar
    main.cpp
)
