// One child island: where it sits in the parent, and what it shows.
struct Entry {
    ChildSiteLink link;
    ContentIsland island;
    Rectangle host;
    EventToken layoutUpdated;
};

struct Model {
    TextBlock hint {u"Here's a ContentIsland in a ScrollViewer. Notice how the content scrolls and clips correctly."};
    WrapPanel panel {horizontalAlignment = HorizontalAlignment::Left, visibility = Visibility::Collapsed};
    std::vector<Rectangle> hosts;
    std::vector<Entry> entries;

    Model() {
        for (int index = 0; index < 6; ++index) {
            hosts.push_back(Rectangle {width = 400, height = 400, radiusX = 8, radiusY = 8, Margin {8}, strokeThickness = 1,
                                       stroke = brushes.Card.StrokeColorDefault, fill = brushes.Card.BackgroundFillColor.Default});
            panel.children().append(hosts.back());
        }
    }

    // The scene of the island: a visual of the compositor of the host, which turns on its own.
    static ContentIsland scene(Compositor const& compositor, Vector2 size) {
        auto const square = compositor.createSpriteVisual();
        square.size({size.x / 2, size.y / 2});
        square.anchorPoint({0.5f, 0.5f});
        square.offset({size.x / 2, size.y / 2, 0.0f});
        square.brush(compositor.createColorBrush(rgb(60, 120, 220)));

        auto const turn = compositor.createScalarKeyFrameAnimation();
        turn.insertKeyFrame(0.0f, 0.0f);
        turn.insertKeyFrame(1.0f, 360.0f, compositor.createLinearEasingFunction());
        turn.duration(std::chrono::seconds {6});
        turn.iterationBehavior(AnimationIterationBehavior::Forever);
        square.startAnimation(u"RotationAngleInDegrees", turn);

        auto const root = compositor.createContainerVisual();
        root.size(size);
        root.children().insertAtTop(square);
        return ContentIsland::create(root);
    }

    void load() {
        panel.visibility(Visibility::Visible);
        if (entries.size() == hosts.size()) return;

        auto const host = hosts[entries.size()];
        auto const parent = host.xamlRoot().contentIsland();
        auto const placement = ElementCompositionPreview::getElementVisual(host).try_as<ContainerVisual>();
        // The panel has just been shown and is not laid out yet: the size is the one the host was given.
        auto const size = Vector2 {static_cast<float>(host.width()), static_cast<float>(host.height())};

        // The link lives as long as the connection between the parent and the child island.
        auto const link = ChildSiteLink::create(parent, placement);

        // The offset of the child in the parent follows the host element, or the automation of the
        // island would look for it in the wrong place. Called at every layout of this thread: little work.
        auto const follow = [link, host]() {
            auto const point = host.transformToVisual(host.xamlRoot().content()).transformPoint({0, 0});
            auto matrix = identity_matrix();
            matrix.m41 = static_cast<float>(point.x);
            matrix.m42 = static_cast<float>(point.y);
            link.localToParentTransformMatrix(matrix);
        };
        auto const token = host.add_onLayoutUpdated([follow](Object const&, Object const&) { follow(); });
        follow();

        placement.size(size);
        link.actualSize(size);

        auto const island = scene(placement.compositor(), size);
        link.connect(island);
        entries.push_back(Entry {link, island, host, token});
    }

    // The page is left: the handlers are taken off and the islands closed, the link last.
    void unload() {
        for (auto& entry : entries) {
            entry.host.remove_onLayoutUpdated(entry.layoutUpdated);
            entry.island.close();
            entry.link.close();
        }
        entries.clear();
    }
};
auto const model = gallery::hold<Model>();

auto example = Grid {
    rowSpacing = 8,
    rowDefinitions = u"auto,auto,*",
    onUnloaded = [model](auto&&...) { model->unload(); },
    model->hint,
    Button {row = 1, styles.Button.Accent, content = u"Load model", onClick = [model](Button const&) { model->load(); }},
    Border {row = 2, model->panel},
};