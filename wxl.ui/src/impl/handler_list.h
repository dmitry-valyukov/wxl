#pragma once

// The handlers of one event of CompositionWindow, and the answer its remove_on... pairs
// give: whether the token named a handler that was there.
//
// A handler is taken off from inside a call as well -- that is how the wait of a coroutine
// that got its event goes, and a WindowWatch destroyed from inside its own call -- so the
// call runs by index with a copy of the handler in hand, and the places taken off are swept
// out once nobody is being called. The list itself has to outlive the call: whoever raises
// the event holds what it lives in.
//
// The tokens are not the list's: one counter for the process gives every subscription to
// any event of any window a token of its own, so a token of one event or window names
// nothing in another.
//
// Included by CompositionWindow.cpp, DrawingSurface.cpp and the tests, after the projection:
// the empty sender is made through Object::Impl.

#include "../Object.impl.h"
#include "../events.h"

namespace wxl::impl {

// The one counter that gives every subscription to any event of any window, and to the
// replacement of any compositor's rendering device, a token of its own. One STA thread, so
// a plain counter; inline, so there is one for the program.
inline EventToken next_event_token() noexcept {
    static std::int64_t last = 0;
    return {++last};
}

template <typename Args>
class handler_list {
public:
    // `token` is the counter's next one, never the empty token.
    EventToken add(EventToken token, EventHandler<Args> handler) {
        entries_.push_back({token.value, std::move(handler)});
        return token;
    }

    // False when the token names no handler of this list: one taken off already, one given
    // to another event or another window, or no token at all. A place taken off during a call
    // keeps no token, so it answers to none.
    bool remove(EventToken token) {
        if (!token) return false;
        for (entry& each : entries_) {
            if (each.token == token.value) {
                each.token = 0;
                each.handler = nullptr;
                if (invocations_ == 0) compact();
                return true;
            }
        }
        return false;
    }

    void fire(EventArgsRef<Args> args) {
        ++invocations_;
        Object const sender = Object::Impl::empty<Object>();
        for (std::size_t at = 0; at < entries_.size(); ++at) {
            if (!entries_[at].handler) continue;
            EventHandler<Args> const handler = entries_[at].handler;
            handler(sender, args);
        }
        if (--invocations_ == 0) compact();
    }

private:
    struct entry {
        std::int64_t token;
        EventHandler<Args> handler;
    };

    void compact() {
        std::erase_if(entries_, [](entry const& each) { return !each.handler; });
    }

    std::vector<entry> entries_;
    // Calls of the list under way, one inside another included: the places taken off are
    // swept out when the last of them returns.
    int invocations_ = 0;
};

}  // namespace wxl::impl
