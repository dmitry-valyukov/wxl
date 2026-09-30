auto output = TextBlock {visibility = Visibility::Collapsed};

auto box = PasswordBox {
    width = 300,
    onPasswordChanged = [output](PasswordBox const& self) {
        auto const password = self.password();
        if (password.empty() || password == u"Password") {
            output.visibility(Visibility::Visible);
            output.text(u"'Password' is not allowed.");
            self.password(u"");
        } else {
            output.text(u"");
            output.visibility(Visibility::Collapsed);
        }
    },
};