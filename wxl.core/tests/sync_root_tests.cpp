#include <gtest/gtest.h>


#include "stress_test.h"

import std;
import wxl.core;

using namespace wxl::core;

namespace {

class synchronized_checker : public synchronized_ {
public:
    synchronized_checker(not_null<wxl::core::sync_root> sync_root)
        : synchronized_(sync_root)
    {}

    using synchronized_::is_synchronized;
};

/// То же, но принимает голый указатель -- это другая перегрузка synchronized_.
class raw_synchronized_checker : public synchronized_ {
public:
    // Имя типа приходится писать полностью: в области класса sync_root -- это
    // унаследованный метод synchronized_::sync_root(), а не тип.
    explicit raw_synchronized_checker(::wxl::core::nullable<::wxl::core::sync_root> root)
        : synchronized_(root)
    {}
};
}

// --run_test=System/SyncRoot/memoryCheck  --log_level=test_suite
STRESS_TEST_CASE(sync_root, memory_check)
{
    sync_root_holder holder(new sync_root());

    ASSERT_EQ(1, holder->ref_count());
}

// --run_test=System/SyncRoot/autoSyncRoot  --log_level=test_suite
STRESS_TEST_CASE(sync_root, auto_sync_root)
{
    auto_sync_root_holder auto_sync_root;
    ASSERT_EQ(1, auto_sync_root->ref_count());
}

// --run_test=System/SyncRoot/synchronizedObject  --log_level=test_suite
STRESS_TEST_CASE(sync_root, synchronized_object)
{
    sync_root_holder holder(new sync_root());
    {
        synchronized_checker synchronized_checker(holder.get());

        ASSERT_EQ(synchronized_checker.sync_root(), holder.get());
        ASSERT_EQ(2, holder->ref_count());

        ASSERT_TRUE(!synchronized_checker.is_synchronized());

        // usual guard
        {
            synchronized_::guard g(synchronized_checker);
            ASSERT_TRUE(synchronized_checker.is_synchronized());
            ASSERT_EQ(2, holder->ref_count());
        }
        ASSERT_TRUE(!synchronized_checker.is_synchronized());
    }

    ASSERT_EQ(1, holder->ref_count());
    {
        std::unique_ptr<synchronized_checker>
            checker(new synchronized_checker(holder.get()));

        ASSERT_TRUE(!checker->is_synchronized());

        // safe guard
        {
            synchronized_::safe_guard g(*checker);
            ASSERT_TRUE(checker->is_synchronized());
            ASSERT_EQ(3, holder->ref_count());

            checker.reset();
            ASSERT_EQ(2, holder->ref_count());
            ASSERT_TRUE(holder->is_synchronized());
        }

        ASSERT_TRUE(!holder->is_synchronized());
    }
}

// auto_sync_root_holder(core::nullable<core::sync_root> ) означает две разные вещи, и различает их только
// null: без аргумента он заводит свой sync_root и принимает ту ссылку, с которой
// тот рождается, а с чужим указателем -- берёт взаймы уже существующий. Во втором
// случае ссылку надо завести свою; отобрав чужую, он убил бы объект раньше его
// владельца. Проверяется именно это, потому что сама запись у обоих случаев одна.

// --run_test=System/SyncRoot/anOwnRootIsBornWithItsSingleReference
STRESS_TEST_CASE(sync_root, an_own_root_is_born_with_its_single_reference)
{
    const auto_sync_root_holder own;

    ASSERT_EQ(1, own->ref_count());
}

// --run_test=System/SyncRoot/aBorrowedRootGetsAReferenceOfItsOwn
STRESS_TEST_CASE(sync_root, a_borrowed_root_gets_a_reference_of_its_own)
{
    sync_root_holder root(new sync_root());
    ASSERT_EQ(1, root->ref_count());

    {
        const auto_sync_root_holder borrowed(root.get().get());

        ASSERT_EQ(2, root->ref_count());
        ASSERT_EQ(borrowed.get(), root.get());
    }

    ASSERT_EQ(1, root->ref_count());
}

// --run_test=System/SyncRoot/synchronizedOverARawExternalRoot
STRESS_TEST_CASE(sync_root, synchronized_over_a_raw_external_root)
{
    sync_root_holder root(new sync_root());

    {
        const raw_synchronized_checker checker(root.get().get());

        ASSERT_EQ(2, root->ref_count());
        ASSERT_EQ(checker.sync_root(), root.get());
    }

    ASSERT_EQ(1, root->ref_count());
}
