module;

#include <string>
#include <vector>

// What the generator knows about one metadata type, in one place: the names
// it is written under, what it derives from, the interfaces it implements,
// and -- of those -- the default one, which is what a call hands the object
// over as, together with the name of the Impl field holding it.
//
// Every stage reads these facts from here instead of asking winmd its own
// questions, so the answer to "what is this type called" or "which field
// stands for it" is the same one everywhere.
//
// The wxl name is the bare TypeName: a wrapper is written flat in
// `namespace wxl`. The winrt name is here because the wrappers are built
// over the cppwinrt projection, and because interoperating with that
// projection stays useful once they are not.

export module wxl.gen:metadata;

import :md;
export namespace gen {

struct type_facts {
    std::string metadata_name;  // "Microsoft.UI.Xaml.Media.SystemBackdrop"
    std::string name;           // "SystemBackdrop" -- namespace wxl is implied
    std::string winrt_name;     // "winrt::Microsoft::UI::Xaml::Media::SystemBackdrop"

    md::category kind{};

    // What the type extends, empty when it extends nothing wxl mirrors
    // (System.Object, System.Enum, a type outside the loaded metadata).
    md::TypeDef base;

    // Every interface the type implements directly. A parameterized one
    // (IVector<T> and friends) has no TypeDef of its own and is left out.
    std::vector<md::TypeDef> interfaces;

    // The interface a call takes the object as. Empty when the class marks
    // none, or marks a parameterized one.
    md::TypeDef default_interface;

    // The Impl field holding that interface: "systemBackdrop_". Empty
    // exactly when default_interface is.
    std::string primary_field;
};

// Computed on first use and kept for the run -- the same type is asked about
// by the crawler and by several writers.
type_facts const& facts_of(md::TypeDef const& type);

}  // namespace gen
