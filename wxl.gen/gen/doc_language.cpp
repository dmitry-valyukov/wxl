#include <ostream>
#include <print>

#include "documentation.h"

#include "wxl.gen.h"

import std;
import wxl.core;

// doc_language.txt: the language the run documented the wrappers in, for the
// documentation site to read. The descriptions in the headers are in that
// language, so the site's own text -- Doxygen's headings and labels -- follows
// it instead of being chosen a second time; the line carries Doxygen's name
// for the language too, so the site keeps no table of its own. A text file, so
// neither the compiler nor Doxygen reads it as a source: the build lists it
// with the rest, as it does aliases.txt, and compiles nothing of it.

namespace gen {

void write_doc_language(output const& out, model const& m, emitted& em) {
    documentation_language const* const language = m.doc_language;
    if (!language) {
        return;
    }

    auto const path = out.dir / "doc_language.txt";
    {
        auto file = open_output(path);
        std::print(file, "{}{} {}\n", banner, language->code, language->doxygen);
    }
    em.add(path);

    std::print("generated {} ({})\n", wxl::core::unicode::to_utf8(path).chars(), language->doxygen);
}

}  // namespace gen
