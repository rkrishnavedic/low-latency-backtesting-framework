#include <gtest/gtest.h>
#include "order_book.hpp"

TEST(FlatOrderBookTest, AddAndQueryOrder) {
    FlatOrderBook book;

    uint32_t idx1 = book.add_order(1001, 50000, 10, Side::BID);
    uint32_t idx2 = book.add_order(1002, 50000, 25, Side::BID);

    EXPECT_NE(idx1, INVALID_INDEX);
    EXPECT_NE(idx2, INVALID_INDEX);

    const LimitLevel& level = book.get_level(50000, Side::BID);
    EXPECT_EQ(level.price, 50000);
    EXPECT_EQ(level.total_volume, 35);
    EXPECT_EQ(level.order_count, 2);
    EXPECT_EQ(level.head_order_idx, idx1);
    EXPECT_EQ(level.tail_order_idx, idx2);
}

TEST(FlatOrderBookTest, CancelHeadOrder) {
    FlatOrderBook book;

    uint32_t idx1 = book.add_order(1001, 50000, 10, Side::BID);
    uint32_t idx2 = book.add_order(1002, 50000, 25, Side::BID);

    bool cancelled = book.cancel_order(idx1);
    EXPECT_TRUE(cancelled);

    const LimitLevel& level = book.get_level(50000, Side::BID);
    EXPECT_EQ(level.total_volume, 25);
    EXPECT_EQ(level.order_count, 1);
    EXPECT_EQ(level.head_order_idx, idx2);
    EXPECT_EQ(level.tail_order_idx, idx2);
}

TEST(FlatOrderBookTest, CancelMiddleOrder) {
    FlatOrderBook book;

    uint32_t idx1 = book.add_order(1001, 50000, 10, Side::BID);
    uint32_t idx2 = book.add_order(1002, 50000, 15, Side::BID);
    uint32_t idx3 = book.add_order(1003, 50000, 20, Side::BID);

    bool cancelled = book.cancel_order(idx2);
    EXPECT_TRUE(cancelled);

    const LimitLevel& level = book.get_level(50000, Side::BID);
    EXPECT_EQ(level.total_volume, 30);
    EXPECT_EQ(level.order_count, 2);
    EXPECT_EQ(level.head_order_idx, idx1);
    EXPECT_EQ(level.tail_order_idx, idx3);

    // Verify doubly-linked pointers between idx1 and idx3
    EXPECT_EQ(book.get_order(idx1).next_order_idx, idx3);
    EXPECT_EQ(book.get_order(idx3).prev_order_idx, idx1);
}