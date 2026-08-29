#include <gtest/gtest.h>
#include "arena_pool.hpp"

struct DummyOrder {
    uint64_t id;
    uint32_t price;
    uint32_t qty;
};

TEST(ArenaPoolTest, CacheLineAlignment) {
    ArenaPool<DummyOrder, 1024> pool;
    // Verify that memory alignment matches 64 bytes
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&pool) % CACHE_LINE_SIZE, 0);
}

TEST(ArenaPoolTest, AllocateAndDeallocate) {
    ArenaPool<DummyOrder, 100> pool;

    uint32_t idx0 = pool.allocate(DummyOrder{101, 50000, 10});
    uint32_t idx1 = pool.allocate(DummyOrder{102, 50050, 20});

    EXPECT_EQ(idx0, 0);
    EXPECT_EQ(idx1, 1);
    EXPECT_EQ(pool[idx0].id, 101);
    EXPECT_EQ(pool[idx1].price, 50050);

    // Deallocate first item
    pool.deallocate(idx0);

    // Next allocation should reuse index 0 (head of free list)
    uint32_t idx2 = pool.allocate(DummyOrder{103, 50100, 30});
    EXPECT_EQ(idx2, 0);
    EXPECT_EQ(pool[idx2].id, 103);
}

TEST(ArenaPoolTest, ExhaustCapacity) {
    ArenaPool<DummyOrder, 2> pool;

    uint32_t idx0 = pool.allocate(DummyOrder{1, 100, 5});
    uint32_t idx1 = pool.allocate(DummyOrder{2, 100, 5});
    uint32_t idx2 = pool.allocate(DummyOrder{3, 100, 5});

    EXPECT_NE(idx0, INVALID_INDEX);
    EXPECT_NE(idx1, INVALID_INDEX);
    EXPECT_EQ(idx2, INVALID_INDEX); // Pool full
}