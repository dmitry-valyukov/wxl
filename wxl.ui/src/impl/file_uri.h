#pragma once

#include "../core.h"

// The local path an address with the file scheme names, read off the text the address
// was written as.
//
// Not off Windows.Foundation.Uri's components: AbsoluteUri and Path unescape what is
// escaped one byte at a time, so a name written as UTF-8 escapes (%D0%9A...) comes out
// as Latin-1 letters, and Path unescapes some escapes (%23) and keeps others (%20). The
// text itself is the only form that says what was meant.

namespace wxl::impl {

/// Puts into `path` what `address` names, and says false -- leaving `path` empty or partial --
/// when it is not a file address or its escapes are not UTF-8. Every spelling of a local file
/// the platform's own Uri accepts is read the same: file:///M:/a.jpg, file:/M:/a.jpg, file://M:/a.jpg, file://localhost/M:/a.jpg and
/// the one with backslashes; file://server/share/a.jpg is the network path
/// \\server\share\a.jpg. Escapes are decoded as UTF-8, characters written as themselves
/// stay; the query and the fragment, which are not part of a file's name, end the path.
bool try_path_of_file_uri(std::u16string_view address, std::u16string& path);

}  // namespace wxl::impl
