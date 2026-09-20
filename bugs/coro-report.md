# x64, optimized, synchronous exception model: a coroutine whose `final_suspend()` returns `suspend_never` skips the destructors of its locals and leaks its coroutine state

## Environment

- Compiler: `Microsoft (R) C/C++ Optimizing Compiler Version 19.51.36257 for x64`
- Toolset: `14.51.36231`, Visual Studio 18 Insiders
- Host/target: Windows 11 Pro 26200, x64
- Command line: `cl /nologo /EHsc /std:c++20 /O2 repro.cpp` (the exception model matters — see below)

## Repro

```cpp
#include <coroutine>
#include <cstdio>

bool destroyed = false;

struct trace {
    ~trace() { destroyed = true; }
};

// Nothing is returned to hold, and the frame releases itself when the body
// ends: both ends of the promise are suspend_never.
struct fire_and_forget {
    struct promise_type {
        fire_and_forget get_return_object() const noexcept { return {}; }
        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_never final_suspend() const noexcept { return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() const noexcept {}
    };
};

// Hands its own handle to the caller and stays suspended.
struct capture {
    std::coroutine_handle<>* out;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> waiter) const noexcept { *out = waiter; }
    void await_resume() const noexcept {}
};

fire_and_forget coro(std::coroutine_handle<>& out) {
    trace held;
    co_await capture{&out};
}

int main() {
    std::coroutine_handle<> waiter;

    coro(waiter);
    waiter.resume();

    std::printf("%s\n", destroyed ? "ok" : "BUG: ~trace() was not called");
    return destroyed ? 0 : 1;
}
```

## Expected

`ok`. The coroutine is resumed, the body runs to its end, control flows off the
end of the coroutine, and `held` — an object with automatic storage duration in
the block being left — is destroyed.

## Actual

With `/O1` or `/O2` on x64 the program prints

```
BUG: ~trace() was not called
```

`~trace()` is never called. Replacing `waiter.resume()` with `waiter.destroy()`
gives the same result: the destructor of the local is skipped on that path too,
although `destroy()` on a suspended coroutine is specified to destroy the
coroutine, and control is then considered transferred out of the function.

## Second symptom: the coroutine state is never deallocated

With a promise that has its own `operator new` / `operator delete`, the traffic
can be counted:

```cpp
struct promise_type {
    static void* operator new(std::size_t size) { ++allocated; return ::operator new(size); }
    static void operator delete(void* mem, std::size_t size) noexcept { ++deallocated; ::operator delete(mem, size); }
    // ... same as above, both ends suspend_never
};
```

```
cl /std:c++20 /EHsc /Od repro-leak.cpp  ->  allocated 1, deallocated 1, ~trace() ran: yes
cl /std:c++20 /EHsc /O2 repro-leak.cpp  ->  allocated 1, deallocated 0, ~trace() ran: no
```

So the frame is allocated, and under optimization it is neither cleaned up nor
released.

## What narrows it down

Same source file, same machine, `cl /nologo repro.cpp <flags>`. "skipped" means
the program printed `BUG: ~trace() was not called`.

**The language level is not involved.** Every level this compiler accepts
behaves the same way, and C++14/17 reach the same state through
`/await:strict`:

| Flags | Result |
|---|---|
| `/std:c++14 /await:strict /EHsc /Od` | ok |
| `/std:c++14 /await:strict /EHsc /O1`, `/O2`, `/Ox` | **skipped** |
| `/std:c++17 /await:strict /EHsc /Od` | ok |
| `/std:c++17 /await:strict /EHsc /O1`, `/O2`, `/Ox` | **skipped** |
| `/std:c++20 /EHsc /Od` | ok |
| `/std:c++20 /EHsc /O1`, `/O2`, `/Ox` | **skipped** |
| `/std:c++23preview /EHsc /Od` | ok |
| `/std:c++23preview /EHsc /O1`, `/O2`, `/Ox` | **skipped** |
| `/std:c++latest /EHsc /Od` | ok |
| `/std:c++latest /EHsc /O1`, `/O2`, `/Ox` | **skipped** |

`/std:c++23`, `/std:c++26`, `/std:c++26preview` and `/std:c++2c` are not options
of this compiler (`D9002`); `/std:c++23preview` and `/std:c++latest` are the
newest levels available here. The build this was found in uses `/std:c++latest`.

**Optimization and inlining.** `/Od` is correct; every optimizing level is not,
and turning inline expansion off restores correct behaviour:

| Flags | Result |
|---|---|
| `/std:c++latest /EHsc /Od` | ok |
| `/std:c++latest /EHsc /O1` | **skipped** |
| `/std:c++latest /EHsc /O2` | **skipped** |
| `/std:c++latest /EHsc /Ox` | **skipped** |
| `/std:c++latest /EHsc /O2 /Ob0` | ok |
| `/std:c++latest /EHsc /O2 /Ob1`, `/Ob2`, `/Ob3` | **skipped** |

**The exception model decides it.** This is the sharpest boundary found: the
synchronous models are broken, the asynchronous ones are correct — and under
`/EHa` the coroutine state is deallocated as well:

| Flags | Result |
|---|---|
| `/std:c++latest /O2 /EHs` | **skipped** |
| `/std:c++latest /O2 /EHsc` | **skipped** |
| `/std:c++latest /O2 /EHa` | ok |
| `/std:c++latest /O2 /EHac` | ok |
| `/std:c++latest /O2 /EHa` (repro-leak.cpp) | allocated 1, deallocated 1, `~trace()` ran |
| `/std:c++latest /O2 /EHsc` (repro-leak.cpp) | allocated 1, deallocated 0, `~trace()` did not run |

**Target architecture.** The x86 compiler of the same toolset is correct at
`/Od`, `/O1` and `/O2`; only x64 is affected.

**Flags that change nothing** (all tested on top of `/std:c++latest /EHsc /O2`,
all still skipped): `/Os`, `/Ot`, `/GL`, `/MT`, `/MD`, `/GS-`, `/permissive-`,
`/Gy`, `/Gw`, `/guard:cf`, `/Qspectre`, `/arch:AVX2`, `/Zi`, `/GR-`,
`/Zc:preprocessor`.

**The shape of the coroutine matters, the handle does not.** In a slightly
larger test program, changing only `final_suspend()` to return
`std::suspend_always` — the coroutine is then destroyed by its owner — makes
every case pass under `/O2`. Whether `destroy()` is called through
`std::coroutine_handle<>` or through `std::coroutine_handle<promise_type>` makes
no difference.

## What restores the cleanup: anything in the body that can throw

This is the sharpest boundary of all. `repro-throw.cpp` (attached) holds six
variants of the same self-owning coroutine, differing only in whether the
compiler must assume an exception can arise in the body. Built as
`cl /nologo /std:c++latest /EHsc /O2 repro-throw.cpp helper.cpp`, where
`helper.cpp` holds `void may_throw() {}` and `volatile bool never = false`, so
that neither can be seen through:

| Coroutine body | Result |
|---|---|
| nothing in it can throw (all awaiter and promise members `noexcept`) | **destructor skipped** |
| an unreachable `throw`: `if (never) throw std::runtime_error("never");` | destructor runs |
| a call to `may_throw()`, defined in another translation unit | destructor runs |
| the `co_await` wrapped in `try { … } catch (...) { }` | destructor runs |
| the awaiter's members not `noexcept` | destructor runs |
| the promise's members not `noexcept` (`final_suspend()` has to stay `noexcept`, [dcl.fct.def.coroutine]/15) | destructor runs |

The coroutine state follows the destructor. With the counting promise of
`repro-leak.cpp` and a `may_throw()` call added to its body:

```
cl /std:c++latest /EHsc /O2 repro-leak.cpp helper.cpp        ->  allocated 1, deallocated 0, ~trace() ran: no
cl /std:c++latest /EHsc /O2 repro-leak-throw.cpp helper.cpp  ->  allocated 1, deallocated 1, ~trace() ran: yes
```

Under `/EHa /O2`, under `/EHsc /O2 /Ob0` and under `/EHsc /Od`, every row of that
table runs the destructor.

So the defect shows exactly when the compiler may conclude that nothing in the
coroutine body can throw. That is the condition the `/EH` documentation
describes for the synchronous model — "the compiler assumes that exceptions can
only occur at a `throw` statement or at a function call[, which] allows the
compiler to eliminate code for tracking the lifetime of many unwindable
objects" — and with it the ordinary, non-exceptional cleanup of the coroutine
goes too: the destructor of the local and the call to the deallocation
function.

## Standard references

- [coroutine.handle.resumption]/4–5: `void destroy() const;` — *Preconditions*:
  `*this` refers to a suspended coroutine. *Effects*: Destroys the coroutine.
- [dcl.fct.def.coroutine]/11: "The coroutine state is destroyed when control
  flows off the end of the coroutine or the destroy member function of a
  coroutine handle that refers to the coroutine is invoked. In the latter case,
  control in the coroutine is considered to be transferred out of the function.
  The storage for the coroutine state is released by calling a non-array
  deallocation function."
- [basic.stc.auto]/1: the storage for automatic variables lasts until the block
  in which they are created exits — so leaving the body destroys `held` on both
  paths.

## Why the exception model must not decide this

The program throws nothing, and no exception path is involved on either route
through the coroutine: the body is left by flowing off its end, or the frame is
taken apart by `destroy()`. [intro.abstract]/1 requires a conforming
implementation to "emulate (only) the observable behavior of the abstract
machine", and what `~trace()` writes is printed, so it is observable
([intro.abstract]/8). `/EHs`, `/EHsc`, `/EHa` and `/EHac` must therefore all
print `ok`.

The documented difference between the models is about asynchronous (structured)
exceptions, which the C++ standard does not describe at all: the /EH
documentation says that under `/EHs` "objects in scope when an asynchronous
exception occurs aren't destroyed". That is a licensed difference, and it does
not apply here — nothing asynchronous happens in this program.

What the same page says next does look related to the defect: "When you use
`/EHs` or `/EHsc`, the compiler assumes that exceptions can only occur at a
`throw` statement or at a function call. This assumption allows the compiler to
eliminate code for tracking the lifetime of many unwindable objects, which can
significantly reduce code size." The table in "What restores the cleanup" tests
exactly that condition: the moment anything in the body can throw — an
unreachable `throw`, an opaque call, a `try` block, an awaiter or promise
without `noexcept` — the destructor and the deallocation come back. In this
coroutine the elimination therefore appears to
take the ordinary, non-exceptional cleanup with it — the destructor of the local
and the call to the deallocation function. Which part of the back end does that
is of course a guess from the outside; the measurements are the two tables
above.

## Impact

This is the ordinary shape of a fire-and-forget coroutine — the one used for
event handlers and UI work items, where the coroutine owns itself and nobody
holds a handle to it. Under optimization every such coroutine silently leaks its
frame and skips the destructors of everything its body held: subscriptions,
locks, buffers. Debug builds behave correctly, so the difference only appears in
shipping configurations.

## Workaround

Building the translation unit with an asynchronous exception model (`/EHa` or
`/EHac`) produces correct code, as does `/O2 /Ob0` and, of course, `/Od`. Each
of those changes what the rest of the translation unit compiles to, so none is
a neutral substitute for the synchronous model this code is written against.
