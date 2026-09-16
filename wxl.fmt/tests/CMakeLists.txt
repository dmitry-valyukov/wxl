wxl_add_gtest(wxl.fmt.tests
    SRC
        text_builder_tests.cpp
    DEPS
        wxl::fmt
        # The STA pool the pooled builder is tested on.
        wxl::core
)
