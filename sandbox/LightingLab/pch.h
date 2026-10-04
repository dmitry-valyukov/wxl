#pragma once

// Заголовки, которые нужны каждому файлу стенда: декларативная поверхность,
// привязка, поверхность для своего рисования и обёртки композитора -- свет,
// эффекты, тени.

#include "ui.h"
#include "Bind.h"
#include "Panels.h"
#include "DrawingSurface.h"
#include "Button3DEffect.h"
#include "RevealEffect.h"
#include "launch.h"
#include "impl/hresult.h"
#include <wxl/brushes.h>
#include <wxl/Microsoft.Graphics.Canvas.Effects.h>
#include <wxl/Microsoft.UI.Composition.h>
#include <wxl/Microsoft.UI.Composition.Effects.h>
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Hosting.h>
#include <wxl/Microsoft.UI.Xaml.Media.h>
#include <wxl/Windows.Graphics.Effects.h>
