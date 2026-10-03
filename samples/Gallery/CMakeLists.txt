# Перенос WinUI 3 Gallery на wxl: те же типы, без XAML. Страница контрола — по
# .cpp; общая раскладка примера (ControlExample оригинала) — в Presenter.cpp;
# окно с NavigationView — в main.cpp.

# Сниппеты: код примеров, который и компилируется (#include "X.h"), и
# показывается (#include "X.h.embed" — байты того же файла, см. embed.cmake),
# и введения (X.html, только показываются).
file(GLOB_RECURSE _gallery_snippets CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/Snippets/*.h"
     "${CMAKE_CURRENT_SOURCE_DIR}/Snippets/*.html")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Snippets" PREFIX "Snippets"
             FILES ${_gallery_snippets})

set(_gallery_embedded "")
foreach(_snippet IN LISTS _gallery_snippets)
    file(RELATIVE_PATH _relative "${CMAKE_CURRENT_SOURCE_DIR}" "${_snippet}")
    set(_embedded "${CMAKE_CURRENT_BINARY_DIR}/${_relative}.embed")
    add_custom_command(
        OUTPUT "${_embedded}"
        COMMAND "${CMAKE_COMMAND}" "-DIN=${_snippet}" "-DOUT=${_embedded}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/embed.cmake"
        DEPENDS "${_snippet}" "${CMAKE_CURRENT_SOURCE_DIR}/embed.cmake"
        COMMENT "Сниппет ${_relative}.embed"
        VERBATIM)
    list(APPEND _gallery_embedded "${_embedded}")
endforeach()

wxl_add_executable(sample.gallery
    Pages.h
    Catalog.h
    Shell.h
    pch.h
    main.cpp
    Presenter.cpp
    Catalog.cpp
    Contact.cpp
    CustomDataObject.cpp
    Layouts.cpp
    MotionPages.cpp
    LottieLogo.cpp
    NavigationPages.cpp
    TabViewWindowing.cpp
    PagedFrame.cpp
    RepeaterData.cpp
    Shell.cpp
    Tiles.cpp
    ItemPages.cpp
    ItemPage.cpp
    ListPages.cpp
    HomePage.cpp
    SettingsPage.cpp
    ButtonPage.cpp
    DropDownButtonPage.cpp
    HyperlinkButtonPage.cpp
    RepeatButtonPage.cpp
    ToggleButtonPage.cpp
    SplitButtonPage.cpp
    ToggleSplitButtonPage.cpp
    CheckBoxPage.cpp
    ColorPickerPage.cpp
    ComboBoxPage.cpp
    RadioButtonPage.cpp
    RatingControlPage.cpp
    SliderPage.cpp
    ToggleSwitchPage.cpp
    TextBlockPage.cpp
    TextBoxPage.cpp
    PasswordBoxPage.cpp
    NumberBoxPage.cpp
    AutoSuggestBoxPage.cpp
    BorderPage.cpp
    CanvasPage.cpp
    ExpanderPage.cpp
    GridPage.cpp
    RelativePanelPage.cpp
    SplitViewPage.cpp
    StackPanelPage.cpp
    VariableSizedWrapGridPage.cpp
    ViewboxPage.cpp
    DesignPages.cpp
    Design.cpp
    AnimatedVisualPlayerPage.cpp
    CaptureElementPreviewPage.cpp
    InkCanvasPage.cpp
    MapControlPage.cpp
    MediaPlayerElementPage.cpp
    SoundPage.cpp
    PersonPicturePage.cpp
    ImagePage.cpp
    TabViewPage.cpp
    NavigationViewPage.cpp
    PivotPage.cpp
    SelectorBarPage.cpp
    BreadcrumbBarPage.cpp
    ConnectedAnimationPage.cpp
    XamlCompInteropPage.cpp
    ParallaxViewPage.cpp
    PageTransitionPage.cpp
    ThemeTransitionPage.cpp
    ImplicitTransitionPage.cpp
    EasingFunctionPage.cpp
    TeachingTipPage.cpp
    PopupPage.cpp
    FlyoutPage.cpp
    ContentDialogPage.cpp
    ItemsRepeaterPage.cpp
    ItemsViewPage.cpp
    ListViewPage.cpp
    GridViewPage.cpp
    FlipViewPage.cpp
    TreeViewPage.cpp
    PullToRefreshPage.cpp
    ContentIslandPage.cpp
    JumpListPage.cpp
    StoragePickersPage.cpp
    ClipboardPage.cpp
    AppNotificationPage.cpp
    BadgeNotificationManagerPage.cpp
    CompositionWindowPage.cpp
    SystemBackdropsPage.cpp
    AppWindowPage.cpp
    AppWindowTitleBarPage.cpp
    TitleBarPage.cpp
    SamplePages.cpp
    ColorSelector.cpp
    WindowingPage.cpp
    CompactSizingPage.cpp
    SystemBackdropElementPage.cpp
    AcrylicPage.cpp
    AnimatedIconPage.cpp
    RadialGradientBrushPage.cpp
    LinePage.cpp
    ShapePage.cpp
    IconElementPage.cpp
    ThemeShadowPage.cpp
    ScrollViewPage.cpp
    AnnotatedScrollBarPage.cpp
    PipsPagerPage.cpp
    PagerControlPage.cpp
    ScrollViewerPage.cpp
    CalendarViewPage.cpp
    TimePickerPage.cpp
    DatePickerPage.cpp
    CalendarDatePickerPage.cpp
    StandardUICommandPage.cpp
    XamlUICommandPage.cpp
    SwipeControlPage.cpp
    CommandBarFlyoutPage.cpp
    MenuFlyoutPage.cpp
    MenuBarPage.cpp
    CommandBarPage.cpp
    LayoutPanelPage.cpp
    WrapPanelPage.cpp
    AppBarSeparatorPage.cpp
    AppBarToggleButtonPage.cpp
    AppBarButtonPage.cpp
    ToolTipPage.cpp
    ProgressRingPage.cpp
    ProgressBarPage.cpp
    InfoBarPage.cpp
    InfoBadgePage.cpp
    RichTextBlockPage.cpp
    RichEditBoxPage.cpp
    ${_gallery_snippets}
    ${_gallery_embedded}
    embed.cmake
)

# Байты сниппетов лежат в дереве сборки под тем же путём, что и сами сниппеты.
target_include_directories(sample.gallery PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

wxl_target_assets(sample.gallery Assets)
target_link_libraries(sample.gallery PRIVATE wxl::json)

# LottieLogo.cpp -- код анимации Lottie, написанный на объектах Composition и Win2D самой платформы, -- единственный файл
# приложения, которому нужны заголовки проекции; наружу он отдаёт один wxl::Object. Остальным они остаются закрыты.
set_source_files_properties(LottieLogo.cpp PROPERTIES INCLUDE_DIRECTORIES
    "${WXL_CPPWINRT_PROJECTION_DIR};${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/include;${WXL_WINDOWSAPPSDK_WINUI_DIR}/include")
target_link_libraries(sample.gallery PRIVATE Microsoft::CppWinRT)
