#pragma once

// The one place wxl includes <windows.h> from. Every source that needs it,
// directly or through a Windows SDK header that drags it in (d2d1, dxgi,
// unknwn, ...), includes this header first.

// NODRAWTEXT: DrawText is a method on Direct2D's render target, and windows.h
// would rewrite it to its A/W spelling before d2d1.h is even parsed.
#define NODRAWTEXT
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <stdint.h>
#include <windows.h>

// winbase.h's Win16 alias `GetCurrentTime()` would rewrite the projection's
// Storyboard::GetCurrentTime, dropping its argument; nothing here calls the
// alias.
#undef GetCurrentTime
