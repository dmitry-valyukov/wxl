// Один неон на рамку и надпись: эффект — ручка, и один экземпляр, записанный
// заранее, присоединяется к любому числу элементов.
GaussianBlurEffect const neon {color = rgb(255, 43, 214), blurRadius = 16.0f, gamma = 0.6};

auto example =
Border {
    CornerRadius {8},
    Padding {24, 22},
    background = rgb(11, 7, 20),
    StackPanel {
        orientation.horizontal,
        spacing = 28.0,
        hAlign.center,

        // Кольцо и бокал в нём: светятся обводка кольца и линии картинки.
        Grid {
            Ellipse {
                width = 86,
                height = 86,
                stroke = rgb(184, 251, 255),
                strokeThickness = 4.0,
                GaussianBlurEffect {color = rgb(0, 200, 255), blurRadius = 18.0f, gamma = 0.6},
            },
            Image {
                source = u"Assets/Effects/Coctail.png",
                width = 50,
                height = 50,
                GaussianBlurEffect {color = rgb(0, 200, 255), blurRadius = 10.0f, gamma = 0.6},
            },
        },

        // Рамка со скруглением и надпись в ней: один неон на обоих.
        Grid {
            Rectangle {
                width = 230,
                height = 86,
                radiusX = 16,
                radiusY = 16,
                stroke = rgb(255, 196, 246),
                strokeThickness = 4.0,
                neon,
            },
            TextBlock {
                u"Night Club",
                fontFamily = u"Assets/Effects/neonderthaw.ttf#NeonDerthaw",
                fontSize = 40,
                renderTransformOrigin = {0.5, 0.5},
                renderTransform = RotateTransform {angle = -8.0},
                hAlign.center,
                vAlign.center,
                foreground = rgb(255, 232, 251),
                neon,
            },
        },

        // Залитая лампочка: один ореол с гаммой — тугое ядро и широкий разлёт,
        // на что у Halo уходило два.
        Ellipse {
            width = 28,
            height = 28,
            vAlign.center,
            fill = rgb(255, 246, 176),
            GaussianBlurEffect {color = rgb(255, 160, 0), blurRadius = 36.0f, gamma = 0.4},
        },
    },
};
