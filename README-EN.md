# wxl

*Русская версия: [README.md](README.md)*

**wxl (WinUI XAML-Less)** is an experimental C++23 library for building
Windows desktop applications. It lets you describe a **WinUI** user interface
declaratively, directly in the source code, without resorting to XAML.

The library is built around its own projection generator, **winui-srcgen**:
from `.winmd` metadata and a configuration profile, it produces lightweight
wrappers for only the types you need. All sample applications are classic
unpackaged Win32 executables, ready to run directly from the build directory
with no prior deployment step.

## Documentation

The full documentation is available both as an
[online reference](https://wxl-project.github.io/wxl/index.html) and
locally in the `docs/` directory:

- **Architecture and layers:** the rationale behind the engineering
  decisions and a walkthrough of the structure —
  [`docs/index.md`](docs/index.md) |
  [Web](https://wxl-project.github.io/wxl/index.html)
- **Setup:** instructions for building the library and integrating it into
  other projects — [`docs/building.md`](docs/building.md) |
  [Web](https://wxl-project.github.io/wxl/md_docs_2building.html)
- **Media:** a series of in-depth technical publications on Habr (in
  Russian) — [`docs/articles.md`](docs/articles.md) |
  [Web](https://wxl-project.github.io/wxl/md_docs_2articles.html)

## Architectural manifesto

The official Windows development stack (**WinUI** + C++/WinRT + XAML) has three
fundamental drawbacks:

- **A split language environment:** the UI is described in XAML, which
  requires an intermediate compiler, IDL file generation, and fragile wiring
  of elements to C++ code by string names.
- **Excessive compiler load:** the heavyweight headers of the `cppwinrt`
  projection have to be parsed by every translation unit that mentions even
  a single UI component.
- **Runtime overhead:** every object is a full-fledged COM component, which
  means constant interface queries (`QueryInterface`) and atomic cross-thread
  reference counting, even though the entire UI runs on a single thread.

The **wxl** architecture offers an alternative approach:

- **WinRT projection isolation.** The heavyweight `cppwinrt` headers are
  compiled only once — when the `wxl` library itself is built. The
  library's public interface does not use internal WinRT types, so building
  the end application does not require WinRT headers, and it builds in
  seconds.
- **True inheritance and thin wrappers.** Wrapper classes in `wxl` inherit
  from one another exactly as their **WinUI** counterparts do (`wxl::Button` →
  `wxl::ButtonBase` → … → `wxl::UIElement`). As a result, an object is
  passed wherever its base class is expected by native language means, with
  no `.as<T>()`-style casts. The wrapper itself is a single smart pointer to
  the hidden `Impl` implementation chain. No class in the public hierarchy adds
  data members of its own, so every wrapper is always exactly the size of
  one pointer, and copying it by value into a base type is perfectly safe
  and causes no destructive object slicing.
- **A declarative DSL in plain C++.** The UI tree is written as a single C++
  expression: properties, child elements, presets (`Preset`), templates
  (`Template`), event handlers and reactive bindings (`Bind`) are passed as
  constructor arguments and applied to the object in the order they are
  written. The compiler checks the types passed in: an unsuitable argument
  is rejected at build time by a `static_assert` that prints a hint on how
  to fix it. Thanks to `constexpr` and template parameters, constant presets
  and element repeaters (by count or by string literal) are expanded at
  compile time. Moreover, events can be not only handled but also awaited
  natively inside coroutines with `co_await onClick(button)`.
- **Optimized for the single-threaded apartment (STA).** By specification,
  the **WinUI** user interface runs on a single STA thread: UI tree objects (XAML
  elements, windows, compositor visuals) are called only from the thread
  that created them. Because of this, every internal entity that `wxl`
  creates itself — `Impl` objects, event nodes and coroutine frames — uses
  non-atomic reference counts and is allocated from the thread's memory pool
  (`sta_memory_pool`), which is free of locks and atomic operations. The
  **WinUI** components themselves remain standard COM objects. Blocking
  operations (I/O, waits) are handed off to the worker thread of the
  `sta_loop` dispatcher, and their results are delivered back to the UI
  thread without stalling it.
- **Inversion of control over the entry point.** The `wWinMain` function is
  encapsulated inside `wxl.ui`. The library itself initializes the Windows
  App Runtime, sets up the STA thread with its memory pool, creates the XAML
  application object and loads the control resources — and only then hands
  control to the application's one and only function, `wxl_launched()`.
  Unlike a classic `main`, which lives for the entire lifetime of the
  process, `wxl_launched()` follows an event-driven startup and shutdown
  model: its job is to describe the window declaratively and return a
  `Teardown` (a teardown handler in the form of a functor or lambda; it may
  be empty if the application needs no special shutdown handling). The
  library stores it and calls it when the application closes, passing the
  reason — a normal close or a failure — and the handler's result becomes
  the process exit code. The developer does not need to write a message
  loop, a bootstrapper or an `Application` subclass, and all runtime DLLs
  are copied into the application's output directory by the bundled CMake
  scripts.

For a detailed discussion of the architectural rationale and the design of
the projection generator, see the
[first article of the series on Habr](https://habr.com/ru/articles/1080690/).

## Implementation examples

Below is a fragment of a quadratic equation solver from
[samples/Quadratic/main.cpp](samples/Quadratic/main.cpp). The architecture
is built on binding reactive `observable` containers (business logic) to UI
properties through declarative bindings.

```cpp
class Equation : public noncopyable
{
    observable<Answer> answer;

public:
    observable<double> a{blank}, b{blank}, c{blank};
    observable<u16_text> D, x1, x2;

    Equation() {
        // Declarative binding: three input numbers -> solve is called automatically
        answer
            .follow(a, b, c, solve)
            .on_change([this](Answer const& answer) noexcept {
                D.set(answer.D);
                x1.set(answer.x1); // State update -> shown in the UI automatically
                x2.set(answer.x2);
            });
    }
};

// A preset that unifies the styling of input fields
const Preset input {
    common,
    width = 200,
};

// ... Elements initialized inside the window's declarative tree:
NumberBox {
    input,
    placeholderText = u"a",
    intermediateValue = BindInput {eq.a},
},
// ...
TextBox {
    output,
    text = BindOutput {eq.D},
},
```

The full set of sample applications (from a calculator to complex **WinUI**
Gallery components), with a detailed description of the syntax, can be found
in [samples/README.md](samples/README.md) and
[docs/dsl.md](docs/dsl.md).

## Quick start

### Requirements

To build the library and the sample projects, you will need:

- **OS:** Windows x64
- **Compiler toolchain:** Visual Studio 2026 (Insiders channel) with the
  MSVC toolset 14.53.36911 or newer
- **WinUI tools:** in the same installation, the workload "WinUI application
  development" with the component "C++ WinUI app development tools"; in
  Build Tools for Visual Studio 2026, the workload "WinUI application
  development build tools" with the component "C++ WinUI app development
  build tools". The configure gets the NuGet packages and the cppwinrt
  projection from the MSBuild of that installation
- **Windows SDK:** 10.0.26100.0 or newer
- **Build:** CMake (version 4.3 / 4.4) and Ninja
- **Environment:** PowerShell 7 (`pwsh`)

### Step-by-step setup

Start a `pwsh` session in the root directory of the cloned repository and run
the following commands to prepare the environment, generate the wrappers and
run a sample:

```powershell
# 1. Configure the tree and build the winui-srcgen tool without the documentation site.
#    The configure restores the NuGet packages (pinned in tools/packages.json, resolved into
#    tools/packages.lock.json and listed in build\x64\wxl.packages\restored-packages.json)
#    and builds the cppwinrt projection in the build directory
tools\build.ps1 -Target winui-srcgen -Define WXL_BUILD_DOCS=OFF

# 2. Deploy the experimental Windows App Runtime from the package the configure restored
pwsh -NoProfile -File tools\install-runtime.ps1

# 3. Run the generator to build the full profile of WinUI wrappers
#    from the packages the configure restored
build\x64\wxl.gen\Debug\winui-srcgen.exe --profile wxl.gen\profiles\full.winxl-profile --packages build\x64\wxl.packages\restored-packages.json

# 4. Configure CMake and build the Quadratic sample project
tools\build.ps1 -Configure -Target sample.quadratic

# 5. Run the built application
build\x64\samples\Quadratic\Debug\sample.quadratic.exe
```

A detailed walkthrough of each automation step, the supported CMake
configuration options, running the unit tests, and a guide to integrating
`wxl` into your own projects are covered in
[docs/building.md](docs/building.md).

## Repository layout

| Directory | Description and modules |
| :--- | :--- |
| **`wxl.core`** | Library core (the `wxl.core` module): the single-threaded STA memory pool, reference counting, reactive `observable` containers, the event model, grapheme-aware UTF-8/UTF-16 text support, file system access, the optimized `nullable` container. |
| **`wxl.async`** | Asynchronous subsystem: `task` coroutines, the `sta_loop` dispatcher with non-blocking file I/O, operation cancellation, low-level lock-free queues, data exchange channels and thread synchronization primitives. |
| **`wxl.fmt`** / **`wxl.logging`** | `text_builder` and string formatting modules built on top of the `fmt` library. Logging subsystem components. |
| **`wxl.xml`** / **`wxl.json`** | Lightweight parsers for XML and JSON. |
| **`wxl.html`** / related | Modules for parsing HTML, BB-code and RSDN text markup for the `HtmlBlock`, `BbBlock`, `RsdnBlock` visual components. The `wxl.highlight` syntax highlighting subsystem. |
| **`wxl.gen.*`** | Source code of the `winui-srcgen` projection generator (`wxl.gen`, `wxl.gen.common`) and `wxl.gen.ui`, a GUI for visually editing code generation profiles (launched via `run-gen-ui.cmd`). |
| **`wxl.ui`** | The **WinUI** graphics stack without XAML: public object wrappers, the declarative DSL syntax, window management, data binding and visual effects. |
| **Auxiliary** | `samples` — sample projects; `sandbox` — isolated experiments with **WinUI** mechanisms; `test-support` — unit testing infrastructure; `cmake`, `tools`, `docs` — build and automation scripts and documentation sources. |

## Current status and limitations

The project is under active experimental development. Backward compatibility
between individual commits is not guaranteed at this stage. The official
[WinUI Gallery](https://github.com/microsoft/WinUI-Gallery) has been ported
across all control groups; only a few individual pages remain unported, those
that need mechanisms **wxl** does not implement yet or packages outside the
profiles (`SemanticZoom`, `WebView2`).

### MSVC optimizer defect (fixed in toolset 14.53.36911)

In MSVC toolsets from 14.51 up to build 14.53.36911, the **Release**
configuration (`/O1` or `/O2` together with `/EHsc`) was built with a
defect: the compiler did not call the destructors of local variables of a
coroutine whose `final_suspend` returns `std::suspend_never`, and did not
free its frame, if it decided that the coroutine body does not throw. The
body of an `async::task` nobody awaits also ends without suspending, and the
defect affected it too; since application shutdown waits until no live
coroutine frames remain, the samples could hang when the window was closed
in `Release`.

Toolset 14.53.36911 fixes the defect: the repro from the report, under
`/O2 /EHsc` with `/std:c++20` and with `/std:c++latest`, runs the destructors
and frees the frame (verified in October 2026). Release is again built and
tested on a par with Debug.

- **On older toolsets** the workaround still applies: build in the **Debug**
  configuration, without optimization (`/Od`), or with the asynchronous
  exception model (`/EHa`). Detailed report:
  [`bugs/coro-report.md`](bugs/coro-report.md).

### Code generation limitations

- **Profile contents.** **wxl** contains only those **WinUI** types and members that
  are listed in the `wxl.gen/profiles/full.winxl-profile` profile. To add anything
  missing, extend the profile and regenerate the wrappers.
- **Inheritance with overriding (Overrides).** A custom subclass of a **WinUI**
  class that overrides methods through the `I<Class>Overrides` interfaces is
  not supported yet: the COM interface aggregation prototype in
  `sandbox/Overrides` does not build. For custom layout containers, the
  `CustomLayout` and `CustomVirtualizingLayout` classes serve as an interim
  alternative.

## Licensing

The library is distributed under the free **GNU GPL version 3** license.
The full text of the license agreement is in the [LICENSE](LICENSE) file.

For commercial licensing, closed-source use or alternative licensing
options, please contact the author directly by email: `vdimas@mail.ru`.
