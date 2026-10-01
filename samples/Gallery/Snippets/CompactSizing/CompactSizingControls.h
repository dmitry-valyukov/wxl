// The form in one size. What the framework calls compact is a dictionary of
// styles it ships; merging it into the resources of a panel is all it takes
// for the controls inside to be drawn denser. The panel is rebuilt on
// every switch, so the state of the fields is carried over by hand.
struct Form {
    TextBox firstName {header = u"First Name:"};
    TextBox lastName {header = u"Last Name:"};
    PasswordBox password {header = u"Password:"};
    PasswordBox confirmPassword {header = u"Confirm Password:"};
    DatePicker chosenDate {header = u"Pick a date"};
    FrameworkElement panel;

    explicit Form(bool compact)
        : panel {Grid {
              dsl::resources = compact ? ResourceDictionary {source = u"ms-appx:///Microsoft.UI.Xaml/DensityStyles/Compact.xaml"}
                                       : ResourceDictionary {},
              StackPanel {
                  spacing = 8,
                  dsl::resources = ResourceDictionary {
                      entry = Resource {u"TextBoxTopHeaderMargin", Thickness {0, 2}},
                      entry = Resource {u"PasswordBoxTopHeaderMargin", Thickness {0, 2}},
                  },
                  TextBlock {fontSize = 18, compact ? u"Compact Size" : u"Standard Size"},
                  firstName,
                  lastName,
                  password,
                  confirmPassword,
                  chosenDate,
              },
          }} {}

    void copyFrom(Form const& other) {
        firstName.text(other.firstName.text());
        lastName.text(other.lastName.text());
        password.password(other.password.password());
        confirmPassword.password(other.confirmPassword.password());
        chosenDate.date(other.chosenDate.date());
    }
};

struct Model {
    Border host;
    std::unique_ptr<Form> form;

    void show(bool compact) {
        auto next = std::make_unique<Form>(compact);
        if (form) {
            next->copyFrom(*form);
        }
        host.child(next->panel);
        form = std::move(next);
    }
};
auto const model = gallery::hold<Model>();

auto example = model->host;

auto options = RadioButtons {
    header = u"Fluent Standard and Compact Sizing",
    RadioButton {content = u"Standard"},
    RadioButton {content = u"Compact"},
    selectedIndex = 0,
    onSelectionChanged = [model](RadioButtons const& self) { model->show(self.selectedIndex() == 1); },
    onLoaded = [model](RadioButtons const&) { model->show(false); },
};