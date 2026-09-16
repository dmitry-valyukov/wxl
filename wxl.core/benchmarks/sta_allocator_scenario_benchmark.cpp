// The workload the sta pools are actually built for: a small tree of nested containers, built,
// churned and torn down over and over -- document holds elements, an element holds attributes
// and leaves, a leaf holds attributes again, and every one of them carries a string. Nothing
// here is a synthetic allocation loop; the sizes, the growth steps and the order of the frees
// are whatever the containers themselves decide.
//
// That order is the whole point. Allocate-all-then-free-all (sta_allocator_benchmark.cpp) never
// makes an allocation follow a free of the same size class. Churning a tree does the opposite:
// erasing a subtree hands whole runs of blocks back, and rebuilding it takes them straight out
// again, which is what the free lists actually see in a running application.
//
// Both variants run the same deterministic workload, interleaved round by round so that
// neither gets the machine in a systematically better state than the other, and the best round
// of each is what gets reported. The allocation count printed alongside comes from a
// separate untimed pass over the identical workload, so ns/alloc is comparable across variants.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

import wxl.core;

namespace {

// ---------------------------------------------------------------------------------------
// The model: four types containing one another, all of them allocator-parameterised.

template <template <typename> class Alloc>
struct model {
    template <typename T>
    using vector = std::vector<T, Alloc<T>>;

    using string = std::basic_string<char, std::char_traits<char>, Alloc<char>>;

    struct attribute {
        string name;
        string value;
    };

    struct leaf {
        string text;
        vector<attribute> attributes;
    };

    struct element {
        string name;
        vector<attribute> attributes;
        vector<leaf> leaves;
    };

    struct document {
        string name;
        vector<element> elements;
    };
};

// Deterministic and identical for every variant, so both allocate exactly the same sequence of
// sizes. A plain LCG rather than <random>, whose engines differ in cost enough to show up next
// to a pool allocation.
class rng {
public:
    explicit rng(uint32_t seed) noexcept : state_(seed) {}

    unsigned in(unsigned lo, unsigned hi) noexcept {
        state_ = state_ * 1664525u + 1013904223u;
        return lo + (state_ >> 16) % (hi - lo + 1);
    }

private:
    uint32_t state_;
};

struct shape {
    unsigned elements;
    unsigned attributes;
    unsigned leaves;
    unsigned churn_rounds;
};

// ---------------------------------------------------------------------------------------
// Building and churning. Every string is long enough to miss the short-string optimisation,
// which is what makes these functions allocate at all.

template <typename String>
String make_string(rng& r, unsigned lo, unsigned hi) {
    return String(r.in(lo, hi), static_cast<char>('a' + r.in(0, 25)));
}

template <typename Model>
typename Model::attribute make_attribute(rng& r) {
    return {make_string<typename Model::string>(r, 16, 32),
            make_string<typename Model::string>(r, 16, 64)};
}

template <typename Model>
typename Model::leaf make_leaf(rng& r, const shape& s) {
    typename Model::leaf result{make_string<typename Model::string>(r, 24, 200), {}};
    for (unsigned i = 0; i < s.attributes / 2; ++i)
        result.attributes.push_back(make_attribute<Model>(r));
    return result;
}

template <typename Model>
typename Model::element make_element(rng& r, const shape& s) {
    typename Model::element result{make_string<typename Model::string>(r, 16, 40), {}, {}};
    for (unsigned i = 0; i < s.attributes; ++i)
        result.attributes.push_back(make_attribute<Model>(r));
    for (unsigned i = 0; i < s.leaves; ++i)
        result.leaves.push_back(make_leaf<Model>(r, s));
    return result;
}

template <typename Model>
typename Model::document build(rng& r, const shape& s) {
    typename Model::document doc{make_string<typename Model::string>(r, 16, 48), {}};
    doc.elements.reserve(s.elements);
    for (unsigned i = 0; i < s.elements; ++i)
        doc.elements.push_back(make_element<Model>(r, s));
    return doc;
}

// Drops every third element and grows the same number of fresh ones back, so a run of frees is
// immediately followed by a run of allocations out of the same size classes.
template <typename Model>
void churn(typename Model::document& doc, rng& r, const shape& s) {
    for (unsigned round = 0; round < s.churn_rounds; ++round) {
        for (size_t i = doc.elements.size(); i-- > 0;) {
            if (i % 3 == 0)
                doc.elements.erase(doc.elements.begin() + static_cast<ptrdiff_t>(i));
        }

        while (doc.elements.size() < s.elements)
            doc.elements.push_back(make_element<Model>(r, s));
    }
}

// The tree is destroyed inside the measured region on purpose: freeing it is half of what the
// allocator is being asked to do here.
template <template <typename> class Alloc>
double run_once(const shape& s) {
    using Model = model<Alloc>;

    rng r(12345u);

    const auto t0 = std::chrono::steady_clock::now();
    {
        typename Model::document doc = build<Model>(r, s);
        churn<Model>(doc, r, s);
    }
    const auto t1 = std::chrono::steady_clock::now();

    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// ---------------------------------------------------------------------------------------
// How many allocations the workload performs, measured once, untimed. Wraps std::allocator so
// the answer is a property of the workload rather than of any pool.

size_t g_allocation_count = 0;

template <typename T>
class counting_allocator {
public:
    using value_type = T;

    counting_allocator() noexcept = default;

    template <typename U>
    constexpr counting_allocator(const counting_allocator<U>&) noexcept {}

    [[nodiscard]] T* allocate(size_t n) {
        ++g_allocation_count;
        return std::allocator<T>{}.allocate(n);
    }

    void deallocate(T* p, size_t n) noexcept { std::allocator<T>{}.deallocate(p, n); }
};

template <typename T, typename U>
bool operator==(const counting_allocator<T>&, const counting_allocator<U>&) noexcept {
    return true;
}

size_t count_allocations(const shape& s) {
    g_allocation_count = 0;
    run_once<counting_allocator>(s);
    return g_allocation_count;
}

// ---------------------------------------------------------------------------------------

template <typename T>
using sta_allocator = wxl::core::sta_allocator<T>;

struct Results {
    double heap_ms = 0.0;
    double sta_ms = 0.0;
};

Results best_of_interleaved(const shape& s, int repeats) {
    Results best;

    for (int i = 0; i < repeats; ++i) {
        const double heap = run_once<std::allocator>(s);
        const double sta = run_once<sta_allocator>(s);

        if (i == 0 || heap < best.heap_ms)
            best.heap_ms = heap;
        if (i == 0 || sta < best.sta_ms)
            best.sta_ms = sta;
    }

    return best;
}

void print_line(const char* label, double ms, size_t allocations) {
    std::printf("  %-12s: %8.3f ms  (%6.1f ns/alloc)\n", label, ms,
                ms * 1e6 / static_cast<double>(allocations));
}

void bench(const char* label, const shape& s, int repeats) {
    const size_t allocations = count_allocations(s);

    run_once<std::allocator>(s);  // warm-up, discarded
    run_once<sta_allocator>(s);

    const Results best = best_of_interleaved(s, repeats);

    std::printf("%s: %u elements x (%u attributes + %u leaves), %u churn rounds\n", label,
                s.elements, s.attributes, s.leaves, s.churn_rounds);
    std::printf("  allocations : %zu per round\n", allocations);
    print_line("std::alloc", best.heap_ms, allocations);
    print_line("sta_alloc", best.sta_ms, allocations);
    std::printf("  speedup     : %.2fx over std::alloc\n\n", best.heap_ms / best.sta_ms);
}

}  // namespace

int main() {
    constexpr int Repeats = 10;

#ifndef NDEBUG
    std::printf(
        "NOTE: this is a Debug build - the CRT debug heap makes std::allocator\n"
        "artificially slow, and the pools pay for their debug thread checks. Build\n"
        "Release for meaningful numbers.\n\n");
#endif

    std::printf("Nested containers: document > element > {attribute, leaf > attribute}\n");
    std::printf("%d repeats, the two variants interleaved, best round of each\n\n", Repeats);

    bench("wide ", {64, 8, 16, 24}, Repeats);
    bench("deep ", {16, 4, 96, 24}, Repeats);
    bench("small", {128, 2, 2, 64}, Repeats);

    return 0;
}
