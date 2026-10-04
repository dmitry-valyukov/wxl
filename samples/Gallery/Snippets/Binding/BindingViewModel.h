// A view model: the fields are private and the view is given them read-only (observable<T const>), so it can show a
// field and bind to it but only the model sets it.
class ViewModel : public core::noncopyable {
public:
    core::observable<core::u16_text const>& title() { return title_; }
    core::observable<core::u16_text const>& description() { return description_; }

    void welcome() {
        title_.set(core::u16_text {u"Welcome to WinUI 3"});
        description_.set(core::u16_text {u"This is an example of binding to a view model."});
    }
    void farewell() {
        title_.set(core::u16_text {u"Goodbye"});
        description_.set(core::u16_text {u"The model changed its fields, and the view followed."});
    }

private:
    core::observable<core::u16_text> title_, description_;
};
auto const model = gallery::hold<ViewModel>();
model->welcome();
auto* const viewModel = model.get();

auto example = StackPanel {
    spacing = 8.0,
    TextBlock {FontWeight {600}, u"Title:"},
    TextBlock {fontSize = 16, text = BindOutput {model->title()}},
    TextBlock {FontWeight {600}, u"Description:"},
    TextBlock {fontSize = 16, text = BindOutput {model->description()}},
    StackPanel {orientation.horizontal, spacing = 8.0,
                Button {content = u"Welcome", onClick = [viewModel](auto&&...) { viewModel->welcome(); }},
                Button {content = u"Goodbye", onClick = [viewModel](auto&&...) { viewModel->farewell(); }}},
};
