#pragma once

// The single place wxl.ui reaches wxl.core, and the reason every other
// wxl header includes this one instead of importing directly.
//
// wxl.core is a C++20 module built with `import std;`. Loading its BMI makes
// the std module visible in the consuming translation unit, and MSVC cannot
// then make sense of a standard header included afterwards -- the header
// redeclares what the module already declared. So the standard headers wxl's
// own headers go on to use are included *here*, ahead of the import, and
// this header is what every wxl header includes first.
//
// Every translation unit including this is an ordinary (non-module) one
// today, which is what makes a module import inside a header legal at all;
// when wxl.ui itself becomes a module, this import moves to the top of
// its interface unit.

// Every standard header any wxl header goes on to need has to be listed
// here: after the import below, MSVC rejects a standard header that has not
// already been seen. Adding one to a wxl header means adding it here.
//
// The list is wider than what wxl's own headers use, and deliberately so: an
// application's headers are included after this one and go on including
// standard headers of their own, which by then would be too late. Anything
// ordinary enough for an application to reach for belongs here.
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cctype>
#include <chrono>
#include <coroutine>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

import wxl.core;
