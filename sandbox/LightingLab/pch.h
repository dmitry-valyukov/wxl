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
#include "generated/brushes.h"
#include "generated/Microsoft.Graphics.Canvas.Effects.h"
#include "generated/Microsoft.UI.Composition.h"
#include "generated/Microsoft.UI.Composition.Effects.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Hosting.h"
#include "generated/Microsoft.UI.Xaml.Media.h"
#include "generated/Windows.Graphics.Effects.h"
