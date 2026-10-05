// The model owns the brush and the six sliders: switching the mapping mode
// changes what the sliders mean (a fraction of the box, or pixels), so they
// are set again, and every handler reaches them by address.
constexpr double side = 200;

struct Model {

    RadialGradientBrush brush {
        center = Point {0.25, 0.25},
        gradientOrigin = Point {0.5, 0.25},
        mappingMode = BrushMappingMode::RelativeToBoundingBox,
        radiusX = 0.5,
        radiusY = 0.5,
        spreadMethod = GradientSpreadMethod::Pad,
        gradientStops[
            GradientStop {offset = 0.0, color = rgb(255, 255, 0)},
            GradientStop {offset = 1.0, color = colors.blue}
        ],
    };

    Slider centerXSlider = slider(u"Center.X", 1, 0);
    Slider centerYSlider = slider(u"Center.Y", 1, 1);
    Slider radiusXSlider = slider(u"RadiusX", 2, 0);
    Slider radiusYSlider = slider(u"RadiusY", 2, 1);
    Slider originXSlider = slider(u"GradientOrigin.X", 3, 0);
    Slider originYSlider = slider(u"GradientOrigin.Y", 3, 1);

    Slider slider(char16_t const* title, int gridRow, int gridColumn) {
        return Slider {header = title, row = gridRow, column = gridColumn, onValueChanged = [this](Slider const&) { apply(); }};
    }

    static Point point(double x, double y) { return Point {static_cast<float>(x), static_cast<float>(y)}; }

    void apply() {
        brush.center(point(centerXSlider.value(), centerYSlider.value()));
        brush.radiusX(radiusXSlider.value());
        brush.radiusY(radiusYSlider.value());
        brush.gradientOrigin(point(originXSlider.value(), originYSlider.value()));
    }

    // In pixels (Absolute) or in fractions of the rectangle (RelativeToBoundingBox).
    void initSliders(bool absolute) {
        double const top = absolute ? side : 1.0;
        double const middle = top / 2;
        double const stepSize = absolute ? side / 50 : 0.02;
        for (auto const& each : {centerXSlider, centerYSlider, radiusXSlider, radiusYSlider, originXSlider, originYSlider}) {
            each.maximum(top);
            each.stepFrequency(stepSize);
            each.smallChange(absolute ? 10 : 0.05);
            each.value(middle);
        }
    }
};
auto const model = gallery::hold<Model>();

auto example = StackPanel {
    hAlign.center,
    vAlign.center,
    Rectangle {width = side, height = side, fill = model->brush},
};

auto options = Grid {
    rowDefinitions = u"auto,auto,auto,auto,auto",
    columnDefinitions = u"*,*",
    ComboBox {
        columnSpan = 2,
        header = u"MappingMode",
        ComboBoxItem {content = u"RelativeToBoundingBox"},
        ComboBoxItem {content = u"Absolute"},
        selectedIndex = 0,
        onSelectionChanged = [model](ComboBox const& self) {
            bool const absolute = self.selectedIndex() == 1;
            model->brush.mappingMode(absolute ? BrushMappingMode::Absolute : BrushMappingMode::RelativeToBoundingBox);
            model->initSliders(absolute);
        },
    },
    model->centerXSlider,
    model->centerYSlider,
    model->radiusXSlider,
    model->radiusYSlider,
    model->originXSlider,
    model->originYSlider,
    ComboBox {
        row = 4,
        columnSpan = 2,
        Margin {0, 10, 0, 0},
        header = u"SpreadMethod",
        ComboBoxItem {content = u"Pad"},
        ComboBoxItem {content = u"Reflect"},
        ComboBoxItem {content = u"Repeat"},
        selectedIndex = 0,
        onSelectionChanged = [model](ComboBox const& self) {
            static constexpr GradientSpreadMethod methods[] = {
                GradientSpreadMethod::Pad, GradientSpreadMethod::Reflect, GradientSpreadMethod::Repeat};
            model->brush.spreadMethod(methods[self.selectedIndex()]);
        },
    },
    onLoaded = [model](Grid const&) { model->initSliders(false); },
};