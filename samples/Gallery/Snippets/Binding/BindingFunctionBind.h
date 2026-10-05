// A field that follows others: `sum` is a function of `a` and `b`, computed again whenever either changes, and a control
// is bound to it like to any field. This is how the answers of the Quadratic sample come from its three coefficients.
struct Model {
    core::observable<double> a {1}, b {2};
    core::observable<core::u16_text> sum;

    Model() {
        sum.follow(a, b, [](double first, double second) { return core::format(u"{} + {} = {}", first, second, first + second); });
    }
};
auto const model = gallery::hold<Model>();

auto example = StackPanel {
    spacing = 8.0,
    StackPanel {orientation.horizontal, spacing = 8.0,
                NumberBox {header = u"a", value = Bind {model->a}}, NumberBox {header = u"b", value = Bind {model->b}}},
    TextBlock {text = BindOutput {model->sum}},
};
