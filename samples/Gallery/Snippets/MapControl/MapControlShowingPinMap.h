auto const tokenBox = PasswordBox {minWidth = 200, automationName = u"Map service token", placeholderText = u"Map service token"};
auto const map = MapControl {height = 400, hAlign.stretch, automationName = u"Map"};

// The pin and the view are set once the map is on the page, as the page of the original does when it is loaded.
map.add_onLoaded([map](auto&&...) {
    map.center(Geopoint {BasicGeoposition {.latitude = 0, .longitude = 0}});
    map.zoomLevel(1);

    auto const landmarks = MapElementsLayer {};
    landmarks.mapElements().append(MapIcon {location = Geopoint {BasicGeoposition {.latitude = -30.034647, .longitude = -51.217659}}});
    map.layers().append(landmarks);
});

auto const setToken = [map, tokenBox](auto&&...) { map.mapServiceToken(tokenBox.password()); };
tokenBox.add_onKeyDown([setToken](auto const&, KeyRoutedEventArgs& args) {
    if (args.key() == VirtualKey::Enter) {
        setToken();
    }
});

auto example = StackPanel {
    spacing = 12,
    StackPanel {orientation.horizontal, spacing = 8, tokenBox, Button {content = u"Set token", onClick = setToken}},
    map,
};