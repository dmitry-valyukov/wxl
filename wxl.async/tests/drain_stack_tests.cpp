#include <gtest/gtest.h>

import std;
import wxl.core;
import wxl.async;



using namespace wxl::core;
using namespace wxl::async;

namespace {

struct test_node : intrusive_slist_node<test_node> {
    int value;
    explicit test_node(int v = 0) : value(v) {}
};

std::vector<int> collect_values(test_node* chain) {
    std::vector<int> values;
    for (test_node* n = chain; n; n = n->next_)
        values.push_back(n->value);
    return values;
}

}  // namespace

TEST(DrainStackTest, DefaultConstructedStackIsEmpty) {
    drain_stack<test_node> stack;
    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.pop_all(), nullptr);
}

TEST(DrainStackTest, PopAllReturnsPushedNodesInLifoOrder) {
    drain_stack<test_node> stack;
    test_node a(1), b(2), c(3);

    stack.push(&a);
    stack.push(&b);
    stack.push(&c);

    EXPECT_FALSE(stack.empty());

    test_node* chain = stack.pop_all();
    EXPECT_EQ(collect_values(chain), (std::vector<int>{3, 2, 1}));
    EXPECT_TRUE(stack.empty());
}

TEST(DrainStackTest, PopAllOnEmptyStackReturnsNullptr) {
    drain_stack<test_node> stack;
    EXPECT_EQ(stack.pop_all(), nullptr);
}

TEST(DrainStackTest, PopAllClaimsEveryNodePushedConcurrentlyExactlyOnce) {
    drain_stack<test_node> stack;

    constexpr int producers = 8;
    constexpr int per_producer = 2000;
    constexpr int total = producers * per_producer;

    std::vector<test_node> nodes(total);
    for (int i = 0; i < total; ++i)
        nodes[i].value = i;

    std::vector<std::thread> threads;
    for (int p = 0; p < producers; ++p) {
        threads.emplace_back([&, p] {
            for (int i = 0; i < per_producer; ++i)
                stack.push(&nodes[p * per_producer + i]);
        });
    }
    for (auto& t : threads) t.join();

    std::unordered_set<int> seen;
    for (test_node* chain = stack.pop_all(); chain; chain = chain->next_)
        EXPECT_TRUE(seen.insert(chain->value).second) << "duplicate node " << chain->value;

    EXPECT_EQ(seen.size(), static_cast<size_t>(total));
    EXPECT_TRUE(stack.empty());
}

namespace {

struct payload {
    int value;
    explicit payload(int v) : value(v) {}
};

}  // namespace

TEST(AsNodeTest, AdaptsATypeThatDoesNotItselfDeriveFromIntrusiveListNode) {
    using wrapped_node = as_node<payload>;

    drain_stack<wrapped_node> stack;

    wrapped_node a(1);
    wrapped_node b(2);

    stack.push(&a);
    stack.push(&b);

    wrapped_node* chain = stack.pop_all();
    ASSERT_NE(chain, nullptr);
    EXPECT_EQ(chain->value, 2);

    ASSERT_NE(chain->next_, nullptr);
    EXPECT_EQ(chain->next_->value, 1);
}
