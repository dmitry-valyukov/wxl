/**
* \file
* \brief Unit tests for \c wxl::async::pool
* \todo Use fixtures instead of uncertain tests' "preconditions"
*/


#include <gtest/gtest.h>



import std;
import wxl.core;
import wxl.async;

using namespace std;
using namespace wxl::core;
using namespace wxl::async;

namespace {

typedef std::map <size_t /* item ID */, bool> id_map;

// this mape is used to track the item lifetime (when it is created or destroyed.
id_map static_item_map;

size_t volatile static_item_id_counter = 0;

class pool_item;
class pool_type;

/// Test item.
class pool_item : public intrusive_slist_node<pool_item>
{
public:
    const size_t id;

    explicit pool_item(pool_type * owner)
        : id(static_item_id_counter++)
        , owner_(owner) {
        static_item_map[id] = true;
    }

    ~pool_item() {
        static_item_map.erase(id);
    }

    pool_type * owner() const {
        return owner_;
    }

private:
    pool_type * const owner_;
};

// Allocator defined in this file in order to involve the memory tracking mechanism
class item_allocator
{
public:
    explicit item_allocator(pool_type * pool_ref)
        : owner_(pool_ref)
    {}

    pool_item * alloc() {
        return new pool_item(owner_);
    }

    static void free(pool_item * element) {
        delete element;
    }

private:
    pool_type * owner_;
};

typedef pool<pool_item, item_allocator> base_pool;

// PoolType
class pool_type : public base_pool
{
    typedef base_pool base;
    pool_type * avoid_this_warning() {
        return this;
    }
public:
    explicit pool_type(const size_t desired_pool_size)
        : base(desired_pool_size, item_allocator(avoid_this_warning()))
    {}
};

typedef pool_ptr<pool_item, intrusive_owner_deleter<pool_item>> item_ptr_intrusive;
typedef pool_ptr<pool_item, provided_owner_deleter<pool_type>> item_ptr_provided;

template <typename ptr_type>
typename ptr_type::deleter_type create_deleter(pool_type*);

template <>
item_ptr_intrusive::deleter_type create_deleter<item_ptr_intrusive>(pool_type*)
{
    return item_ptr_intrusive::deleter_type();
}

template <>
item_ptr_provided::deleter_type create_deleter<item_ptr_provided>(pool_type* pool)
{
    return item_ptr_provided::deleter_type(pool);
}

using pool_ptr_types = ::testing::Types<item_ptr_intrusive, item_ptr_provided>;

template <typename T>
class PoolPtrTest : public ::testing::Test {};

TYPED_TEST_SUITE(PoolPtrTest, pool_ptr_types);

}  // namespace

//// --run_test=System/poolAndStandardContainors
//TEST(PoolTest, pool_and_standard_containors)
//{
//    PoolType pool(8);
//
//    ItemPtr ptr1(pool.get());
//    EXPECT_TRUE(nullptr != ptr1.get());
//
//    ItemPtr ptr2(ptr1.release());
//    EXPECT_TRUE(nullptr != ptr2.get());
//    EXPECT_TRUE(nullptr ==
//                ptr1.get()); // XXX: pool_ptr does not meet the CopyConstructible requirements!!!
//
//    ItemPtr ptr3(pool.get());
//    EXPECT_TRUE(nullptr != ptr3.get());
//
//    std::vector <MessageBlock> container;
//
//    MessageBlock b1(ptr3);
//    EXPECT_TRUE(nullptr == ptr3.get());
//    EXPECT_TRUE(b1.hasMessage());
//
//    container.push_back(b1);
//
//    MessageBlock b2 = container.front();
//    EXPECT_TRUE(b2.hasMessage());
//    EXPECT_TRUE(! b1.hasMessage()); /// XXX: pool_ptr Should NOT be used with standard containors.
//}

// --run_test=System/poolTest0
TYPED_TEST(PoolPtrTest, pool_test0)
{
    using item_ptr = TypeParam;
    {
        pool_type pool(8);

        EXPECT_EQ(8, pool.preferred_size());
        EXPECT_EQ(-8, pool.underflow_counter());

        static_item_id_counter = 0;
        static_item_map.clear();

        item_ptr ptr(pool.get(), create_deleter<item_ptr>(&pool));
        EXPECT_TRUE(ptr.get() != nullptr);
        EXPECT_EQ(static_item_map.size(), 1u);
        EXPECT_EQ(8, pool.preferred_size());
        EXPECT_EQ(-7, pool.underflow_counter());

        const size_t id = ptr->id;

        // Return to the pool.
        ptr.reset();
        EXPECT_TRUE(ptr.get() == nullptr);
        EXPECT_EQ(static_item_map.size(), 1u);
        EXPECT_EQ(8, pool.preferred_size());
        EXPECT_EQ(-7, pool.underflow_counter());

        // Get from the pool again.
        ptr.reset(pool.get());
        EXPECT_EQ(static_item_map.size(), 1u);
        EXPECT_EQ(ptr->id, id);
        EXPECT_EQ(8, pool.preferred_size());
        EXPECT_EQ(-7, pool.underflow_counter());

        // Create a New element and return the previous element to the pool.
        ptr.reset(pool.get());
        EXPECT_EQ(static_item_map.size(), 2u);
        EXPECT_EQ(8, pool.preferred_size());
        EXPECT_EQ(-6, pool.underflow_counter());
    }

    EXPECT_EQ(static_item_map.size(), 0u);
}

// --run_test=System/poolOverflowTest
TYPED_TEST(PoolPtrTest, pool_overflow_test)
{
    using item_ptr = TypeParam;
    {
        pool_type pool(8);
        EXPECT_EQ(8, pool.preferred_size());
        EXPECT_EQ(-8, pool.underflow_counter());

        static_item_id_counter = 0;
        static_item_map.clear();

        {
            pool_item * buffer[16];

            // make twice more items than preffered size
            for(int i = 0; i < 16; i++)
                buffer[i] = pool.get();

            EXPECT_EQ(pool.underflow_counter(), 8);
            EXPECT_EQ(static_item_map.size(), 16u);

            // return items back to the pool.
            for(int i = 0; i < 16; i++)
                item_ptr ptr(buffer[i], create_deleter<item_ptr>(&pool));

            EXPECT_EQ(pool.underflow_counter(), 8);
            EXPECT_EQ(static_item_map.size(), 16u);
        }

        // roll all items in the overflowed pool
        for(int i = 0; i < 16; i++)
            item_ptr ptr(pool.get(), create_deleter<item_ptr>(&pool));

        // make sure underflowCounter was reset
        EXPECT_EQ(pool.underflow_counter(), 0);
        EXPECT_EQ(static_item_map.size(), 16u);

        // roll all items in overflowed pool again
        for(int i = 0; i < 16; i++)
            item_ptr ptr(pool.get(), create_deleter<item_ptr>(&pool));

        // make sure the half of overflowed items are returned to system memory
        EXPECT_EQ(pool.underflow_counter(), 0);
        EXPECT_EQ(static_item_map.size(), 12u);

        // roll the rest of items again
        for(int i = 0; i < 12; i++)
            item_ptr ptr(pool.get(), create_deleter<item_ptr>(&pool));

        // make sure the half of overflowed items are returned to system memory
        EXPECT_EQ(pool.underflow_counter(), 0);
        EXPECT_EQ(static_item_map.size(), 10u);

        // roll the rest of items again
        for(int i = 0; i < 10; i++)
            item_ptr ptr(pool.get(), create_deleter<item_ptr>(&pool));

        // make sure the half of overflowed items are returned to the system memory again.
        EXPECT_EQ(pool.underflow_counter(), 0);
        EXPECT_EQ(static_item_map.size(), 9u);
    }
    EXPECT_EQ(static_item_map.size(), 0u);
}

TYPED_TEST(PoolPtrTest, safe_pool_test0)
{
    using item_ptr = TypeParam;
    {
        pool_type pool(8);
        static_item_id_counter = 0;
        static_item_map.clear();

        item_ptr ptr(pool.get(), create_deleter<item_ptr>(&pool));
        EXPECT_TRUE(ptr.get() != nullptr);
        EXPECT_EQ(static_item_map.size(), 1u);

        const size_t id = ptr->id;
        // return to pool
        ptr.reset();
        EXPECT_TRUE(ptr.get() == nullptr);
        EXPECT_EQ(static_item_map.size(), 1u);

        // get from pool
        ptr.reset(pool.get());
        EXPECT_EQ(static_item_map.size(), 1u);
        EXPECT_EQ(ptr->id, id);

        // create new element and return one element to pool
        ptr.reset(pool.get());
        EXPECT_EQ(static_item_map.size(), 2u);
    }

    EXPECT_EQ(static_item_map.size(), 0u);
}

TYPED_TEST(PoolPtrTest, safe_pool_test1)
{
    using item_ptr = TypeParam;
    {
        pool_type pool(8);
        static_item_id_counter = 0;
        static_item_map.clear();

        item_ptr ptr(pool.get(), create_deleter<item_ptr>(&pool));

        for(int i = 0; i < 10; i++)
            ptr.reset(pool.get());

        EXPECT_EQ(static_item_map.size(), 2u);
    }
    EXPECT_EQ(static_item_map.size(), 0u);
}

namespace {

struct base_command {
    const size_t id;

    base_command() noexcept
        : id(static_item_id_counter++) {
        static_item_map[id] = true;
    }

    ~base_command() {
        static_item_map.erase(id);
    }
};

typedef safe_pool <base_command> command_pool;
typedef command_pool::element_type command;
typedef pool_ptr <command> command_ptr;


command_ptr get_item(command_pool& pool)
{
    command_ptr ptr(pool.get());

    return std::move(ptr);
}


}

/// \bug CPPCOMMONLIBS-125
TEST(PoolTest, pool_overflow)
{
    command_pool pool(3);
    static_item_id_counter = 0;
    static_item_map.clear();

    EXPECT_EQ(pool.preferred_size(), 3u);

    {
        command_ptr ptr1(get_item(pool));
        command_ptr ptr2(pool.get());
        command_ptr ptr3(pool.get());
        command_ptr ptr4(pool.get());
        command_ptr ptr5(pool.get());

        EXPECT_EQ(static_item_map.size(), 5u);
    }

    //here is the pool overflow

    command_ptr ptr(pool.get());

    for(int i = 0; i < 1000; i++)
        ptr.reset(pool.get());

    //here the pool size should be not greater then the pool preferred size

    //EXPECT_LE(static_item_map.size(), pool.preferredSize());
}

TEST(PoolTest, pool_get)
{
    command_pool pool;
    static_item_id_counter = 0;
    static_item_map.clear();

    EXPECT_EQ(pool.preferred_size(), 256u);

    {
        command_ptr ptr1(pool.get());
        EXPECT_TRUE(nullptr != ptr1.get());
        EXPECT_EQ(static_item_map.size(), 1u);

        command_ptr ptr2(pool.get(true));
        EXPECT_TRUE(nullptr != ptr2.get());
        EXPECT_EQ(static_item_map.size(), 2u);

        command_ptr ptr3(pool.get(false));
        EXPECT_TRUE(nullptr == ptr3.get());
        EXPECT_EQ(static_item_map.size(), 2u);
    }

    EXPECT_EQ(static_item_map.size(), 2u);
}
