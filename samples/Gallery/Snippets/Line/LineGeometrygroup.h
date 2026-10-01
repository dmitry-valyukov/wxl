struct Model {
    core::observable<double> radiusX{30};
    core::observable<double> radiusY{30};
};
auto const model = gallery::hold<Model>();

auto example = Canvas {
    width = 100,
    height = 170,
    StackPanel {
        TextBlock {Margin {0, 0, 0, 15}, u"Composite geometry objects can be created using a GeometryGroup."},
        Path {
            fill = rgb(0xcc, 0xcc, 0xff),
            stroke = colors.black,
            strokeThickness = 4,
            // A composite shape from three geometries.
            data = GeometryGroup {
                fillRule = FillRule::EvenOdd,
                children[
                    LineGeometry {startPoint = Point {10, 10}, endPoint = Point {50, 30}},
                    EllipseGeometry {
                        center = Point {40, 70},
                        radiusX = BindOutput {model->radiusX},
                        radiusY = BindOutput {model->radiusY},
                    },
                    RectangleGeometry {rect = Rect {30, 55, 100, 30}}
                ],
            },
        },
    },
};

auto options = StackPanel {
    width = 220,
    Slider {header = u"RadiusX", isFocusEngagementEnabled = false, minimum = 30.0, maximum = 40.0, smallChange = 1.0, stepFrequency = 0.5, value = Bind {model->radiusX}},
    Slider {header = u"RadiusY", isFocusEngagementEnabled = false, minimum = 30.0, maximum = 50.0, smallChange = 1.0, stepFrequency = 0.5, value = Bind {model->radiusY}},
};