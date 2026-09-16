#include <gtest/gtest.h>

import std;
import wxl.core;


using namespace wxl::core;

namespace {

template <typename T, T mask1, T mask2>
void check_set_reset_read_and_reset() {
    bit_vector<T> bv;

    EXPECT_TRUE(bv.set(mask1));
    EXPECT_EQ(bv.value(), mask1);

    // Setting bits that are already set reports no change.
    EXPECT_FALSE(bv.set(mask1));

    EXPECT_TRUE(bv.set(mask2));
    EXPECT_EQ(bv.value(), static_cast<T>(mask1 | mask2));

    EXPECT_TRUE(bv.reset(mask1));
    EXPECT_EQ(bv.value(), mask2);

    // Resetting bits that are already clear reports no change.
    EXPECT_FALSE(bv.reset(mask1));

    EXPECT_EQ(bv.read_and_reset(mask1), 0);
    EXPECT_EQ(bv.read_and_reset(mask2), mask2);
    EXPECT_EQ(bv.value(), 0);
}

}  // namespace

TEST(BitVectorTest, SetResetReadAndResetInt32) {
    check_set_reset_read_and_reset<int32_t, 0x40000000, 0x00000001>();
}

TEST(BitVectorTest, SetResetReadAndResetUint32) {
    check_set_reset_read_and_reset<uint32_t, 0x80000000U, 0x00000001U>();
}

TEST(BitVectorTest, SetResetReadAndResetInt64) {
    check_set_reset_read_and_reset<int64_t, 0x4000000000000000LL, 0x0000000000000001LL>();
}

TEST(BitVectorTest, SetResetReadAndResetUint64) {
    check_set_reset_read_and_reset<uint64_t, 0x8000000000000000ULL, 0x0000000000000001ULL>();
}

TEST(BitVectorTest, InitialValueIsPreserved) {
    bit_vector<uint32_t> bv(0x00000005U);
    EXPECT_EQ(bv.value(), 0x00000005U);
}
