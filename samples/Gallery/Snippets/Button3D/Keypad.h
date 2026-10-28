// Стенд клавиши: клавиатура четыре на четыре, правый столбец -- клавиши
// действия, и две большие клавиши, на которых форму видно глазом; у нижней
// emboss с обратным знаком. Всё, что у эффекта настраивается, -- на панели
// справа: её поля пишут в сеттеры, и клавиши меняются на ходу. Четыре эффекта
// делят одну пару ламп над указателем.
struct Model {
    HoverLight spot {size = 260, fullSize = 120, height = 30, intensity = 0.4};
    BorderLight edge {size = 340, fullSize = 200, height = 60, intensity = 1.5};
    Button3DEffect numeric {foreground = rgb(237, 239, 242), background = rgb(68, 72, 79), spot, edge};
    Button3DEffect action {foreground = rgb(255, 227, 180), background = rgb(176, 98, 30), spot, edge};
    Button3DEffect large {foreground = rgb(237, 239, 242), background = rgb(68, 72, 79), spot, edge};
    Button3DEffect inverted {foreground = rgb(255, 227, 180), background = rgb(176, 98, 30), dsl::emboss = 1.0, spot, edge};

    // Форма и тело.
    core::observable<double> emboss {-1.0}, rim {3.0}, slope {1.0}, smooth {2.0};
    core::observable<bool> rounded {false};
    core::observable<double> rise {3.0}, travel {5.0};
    // Постоянный свет и тени.
    core::observable<double> shadow {0.7}, azimuth {120.0}, elevation {50.0};
    core::observable<double> holeShadow {1.0}, holeSoftness {1.0}, castBlur {1.0};
    // Нажатие.
    core::observable<bool> pressDivesLamp {true}, pressFlashes {false}, pressMovesLabel {true}, pressShades {false};
    core::observable<double> diveDepth {0.33}, diveGain {2.0}, shadeAmount {0.2}, labelShade {0.15};
    // Материал.
    core::observable<double> specularAmount {0.15}, specularShine {30.0};
    // Лампы над указателем: выключенная светит с яркостью ноль.
    core::observable<bool> spotOn {true}, edgeOn {true};
    core::observable<double> spotSize {260.0}, spotFull {120.0}, spotHeight {30.0}, spotIntensity {0.4};
    core::observable<double> edgeSize {340.0}, edgeFull {200.0}, edgeHeight {60.0}, edgeIntensity {1.5};

    Model() {
        // Поле ведёт сеттер: всех четырёх эффектов разом, а emboss у нижней
        // большой клавиши -- с обратным знаком.
        auto const follow = [](auto& field, auto apply) {
            field.on_change([apply](auto const& value) noexcept { apply(value); });
        };
        auto const all = [this](auto setter) {
            return [this, setter](auto value) {
                for (Button3DEffect const* effect : {&numeric, &action, &large, &inverted}) {
                    (effect->*setter)(value);
                }
            };
        };
        follow(emboss, [this](double value) {
            for (Button3DEffect const* effect : {&numeric, &action, &large}) effect->emboss(value);
            inverted.emboss(-value);
        });
        follow(rim, all(&Button3DEffect::rim));
        follow(slope, all(&Button3DEffect::slope));
        follow(smooth, all(&Button3DEffect::smooth));
        follow(rounded, all(&Button3DEffect::rounded));
        follow(rise, all(&Button3DEffect::rise));
        follow(travel, all(&Button3DEffect::travel));
        follow(shadow, all(&Button3DEffect::shadow));
        follow(azimuth, all(&Button3DEffect::azimuth));
        follow(elevation, all(&Button3DEffect::elevation));
        follow(holeShadow, all(&Button3DEffect::holeShadow));
        follow(holeSoftness, all(&Button3DEffect::holeSoftness));
        follow(castBlur, all(&Button3DEffect::castBlur));
        follow(pressDivesLamp, all(&Button3DEffect::pressDivesLamp));
        follow(pressFlashes, all(&Button3DEffect::pressFlashes));
        follow(pressMovesLabel, all(&Button3DEffect::pressMovesLabel));
        follow(pressShades, all(&Button3DEffect::pressShades));
        follow(diveDepth, all(&Button3DEffect::diveDepth));
        follow(diveGain, all(&Button3DEffect::diveGain));
        follow(shadeAmount, all(&Button3DEffect::shadeAmount));
        follow(labelShade, all(&Button3DEffect::labelShade));
        follow(specularAmount, all(&Button3DEffect::specularAmount));
        follow(specularShine, all(&Button3DEffect::specularShine));

        follow(spotSize, [this](double value) { spot.size(value); });
        follow(spotFull, [this](double value) { spot.fullSize(value); });
        follow(spotHeight, [this](double value) { spot.height(value); });
        follow(spotIntensity, [this](double) { relamp(); });
        follow(spotOn, [this](bool) { relamp(); });
        follow(edgeSize, [this](double value) { edge.size(value); });
        follow(edgeFull, [this](double value) { edge.fullSize(value); });
        follow(edgeHeight, [this](double value) { edge.height(value); });
        follow(edgeIntensity, [this](double) { relamp(); });
        follow(edgeOn, [this](bool) { relamp(); });
    }


    void relamp() const {
        spot.intensity(spotOn.get() ? spotIntensity.get() : 0.0);
        edge.intensity(edgeOn.get() ? edgeIntensity.get() : 0.0);
    }
};
auto const model = gallery::hold<Model>();

// Клавиша стенда: 84 на 64 внутри обводки, скругление тела 8, знаки 22;
// большие клавиши -- 220 на 150. Панель под ними растягивается на ширину
// примера.
auto const key = Preset {CornerRadius {9}, fontSize = 22};

auto example = Border {
    hAlign.stretch,
    vAlign.top,
    CornerRadius {12},
    Padding {16},
    background = SolidColorBrush {color = rgb(30, 31, 34)},
    requestedTheme = ElementTheme::Dark,
    StackPanel {
        spacing = 16.0,
        Grid {
            hAlign.left,
            rowSpacing = 10,
            columnSpacing = 10,
            RowDefinitions<u"66,66,66,66">,
            ColumnDefinitions<u"86,86,86,86">,
            [&](iterate<u"789÷456×123−0,=+"> symbol) {
                return Button {
                    symbol.text(),
                    key,
                    symbol.index % 4 == 3 ? model->action : model->numeric,
                    hAlign.stretch,
                    vAlign.stretch,
                    row = symbol.index / 4,
                    column = symbol.index % 4,
                };
            },
        },
        StackPanel {
            orientation.horizontal,
            spacing = 12.0,
            Button {u"emboss", key, model->large, width = 220, height = 150},
            Button {u"−emboss", key, model->inverted, width = 220, height = 150},
        },
    },
};
// Панель настроек: ползунок, флажок и группа -- те же строки, что у стенда.
auto const slider = [](char16_t const* title, core::observable<double>& field, double low, double high, double step) {
    return Slider {header = title, minimum = low, maximum = high, stepFrequency = step, smallChange = step,
                   value = Bind {field}};
};
// Флажок пишет в поле сам: его IsChecked -- значение с тремя состояниями, и
// двусторонней привязки к bool у него нет.
auto const check = [](char16_t const* title, core::observable<bool>& field) {
    return CheckBox {content = title, isChecked = field.get(), onChecked += [&field] { field.set(true); },
                     onUnchecked += [&field] { field.set(false); }};
};
auto const group = [](char16_t const* title, bool open, auto... rows) {
    return Expander {hAlign.stretch, horizontalContentAlignment = HorizontalAlignment::Stretch, header = title,
                     isExpanded = open, content = StackPanel {spacing = 8.0, rows...}};
};

auto options = StackPanel {
    width = 320,
    spacing = 8.0,
    group(u"Нажатая клавиша", true,
          check(u"Лампа пятна ныряет за пальцем", model->pressDivesLamp),
          check(u"Вспышка пятна", model->pressFlashes),
          check(u"Знак, лицо и внутренний край канта уходят вниз и вправо", model->pressMovesLabel),
          check(u"Клавиша темнеет", model->pressShades),
          slider(u"Высота клавиши над панелью", model->rise, 0.0, 8.0, 0.5),
          slider(u"Ход нажатия", model->travel, 0.0, 8.0, 0.5),
          slider(u"Тень края отверстия: доля полной", model->holeShadow, 0.0, 1.0, 0.05),
          slider(u"Тень края отверстия: полутень", model->holeSoftness, 0.0, 3.0, 0.25),
          slider(u"Падающая тень: размытие, раз", model->castBlur, 0.0, 4.0, 0.1),
          slider(u"Нырок: доля высоты лампы", model->diveDepth, 0.1, 1.0, 0.01),
          slider(u"Нырок: яркость, раз", model->diveGain, 1.0, 4.0, 0.05),
          slider(u"Притемнение клавиши: доля", model->shadeAmount, 0.0, 0.6, 0.01),
          slider(u"Потускнение знака: доля", model->labelShade, 0.0, 0.8, 0.01)),
    group(u"Форма клавиши", true,
          slider(u"emboss: чаша — купол", model->emboss, -1.0, 1.0, 0.05),
          slider(u"Ширина канта", model->rim, 0.0, 16.0, 0.5),
          slider(u"Наклон канта (тангенс)", model->slope, 0.0, 4.0, 0.05),
          ToggleSwitch {header = u"Кант скруглён", isOn = Bind {model->rounded}},
          slider(u"Сглаживание стыка лица с кантом", model->smooth, 0.0, 6.0, 0.25)),
    group(u"Пятно на лице: лампа над указателем", false,
          ToggleSwitch {header = u"Пятно включено", isOn = Bind {model->spotOn}},
          slider(u"Поперечник", model->spotSize, 10.0, 1200.0, 1.0),
          slider(u"Поперечник полного света", model->spotFull, 0.0, 1200.0, 1.0),
          slider(u"Высота лампы", model->spotHeight, 5.0, 600.0, 1.0),
          slider(u"Яркость", model->spotIntensity, 0.0, 4.0, 0.01)),
    group(u"Свет на гранях: лампа над указателем", false,
          ToggleSwitch {header = u"Свет на гранях включён", isOn = Bind {model->edgeOn}},
          slider(u"Поперечник", model->edgeSize, 10.0, 1200.0, 1.0),
          slider(u"Поперечник полного света", model->edgeFull, 0.0, 1200.0, 1.0),
          slider(u"Высота лампы", model->edgeHeight, 5.0, 600.0, 1.0),
          slider(u"Яркость", model->edgeIntensity, 0.0, 6.0, 0.01)),
    group(u"Постоянный свет", false,
          slider(u"shadow: доля лампы на плоском месте", model->shadow, 0.0, 1.0, 0.01),
          slider(u"Азимут лампы", model->azimuth, 0.0, 360.0, 1.0),
          slider(u"Высота лампы, градусы", model->elevation, 10.0, 90.0, 1.0)),
    group(u"Материал", false,
          slider(u"Блик: specularAmount", model->specularAmount, 0.0, 2.0, 0.01),
          slider(u"Блик: specularShine", model->specularShine, 1.0, 200.0, 1.0)),
};
