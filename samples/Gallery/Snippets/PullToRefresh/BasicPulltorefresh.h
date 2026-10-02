struct Model {
    ListView list {height = 200, minWidth = 200, borderThickness = 1, borderBrush = brushes.Card.StrokeColorDefault};
    // The refresh is finished when the deferral is completed: until then the visualizer goes on.
    core::nullable<Deferral> pending;
    DispatcherQueueTimer timer = DispatcherQueue::getForCurrentThread().createTimer();
    int added = 0;

    void fill(std::initializer_list<char16_t const*> names) {
        for (auto const name : names) list.items().append(stringBox(name));
    }

    // "Some work to show new content": half a second, and an item at the top of the list.
    void work() {
        timer.start();
    }

    void done() {
        timer.stop();
        list.items().insertAt(0, stringBox(u"NewControl"));
        if (pending) {
            pending->complete();
            pending.reset();
        }
    }
};
auto const model = gallery::hold<Model>();
model->timer.interval(std::chrono::milliseconds {500});
model->timer.isRepeating(false);
model->timer.add_onTick([weak = std::weak_ptr<Model>(model)](auto&&...) {
    if (auto const model = weak.lock()) model->done();
});
model->fill({u"AcrylicBrush", u"ColorPicker", u"NavigationView", u"ParallaxView", u"PersonPicture", u"PullToRefreshPage",
             u"RatingsControl", u"RevealBrush", u"TreeView"});
auto example = RefreshContainer {
    horizontalAlignment = HorizontalAlignment::Center,
    verticalAlignment = VerticalAlignment::Center,
    onRefreshRequested = [model](RefreshContainer const&, RefreshRequestedEventArgs& args) {
        model->pending = args.getDeferral();
        model->work();
    },
    content = model->list,
};