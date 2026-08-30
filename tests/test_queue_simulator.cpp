#include <gtest/gtest.h>
#include "queue_simulator.hpp"

using namespace low_latency;

TEST(QueueSimulatorTest, RejectsFillWhenQueueAheadIsActive) {
    QueueFillSimulator<16> sim;
    FillEvent fills[4];

    // Order 101: BID 100 shares @ $100. Volume ahead = 50 shares
    sim.add_order(101, Side::BID, 100, 100, 50);

    // Trade 1: ASK (sell) 30 shares @ $100. Should consume 30 of 50 volume ahead, 0 fills
    size_t count = sim.process_market_trade(Side::ASK, 100, 30, 1000, fills, 4);
    EXPECT_EQ(count, 0u);
}

TEST(QueueSimulatorTest, FillsPartialAndRemainderAcrossTrades) {
    QueueFillSimulator<16> sim;
    FillEvent fills[4];

    // Order 102: BID 100 shares @ $100. Volume ahead = 50 shares
    sim.add_order(102, Side::BID, 100, 100, 50);

    // Trade 1: ASK (sell) 80 shares @ $100. Consumes 50 ahead, 30 remaining fills strategy order
    size_t count = sim.process_market_trade(Side::ASK, 100, 80, 1000, fills, 4);
    ASSERT_EQ(count, 1u);
    EXPECT_EQ(fills[0].order_id, 102u);
    EXPECT_EQ(fills[0].fill_qty, 30u);

    // Trade 2: ASK (sell) 100 shares @ $100. Fills remaining 70 shares of order 102
    count = sim.process_market_trade(Side::ASK, 100, 100, 2000, fills, 4);
    ASSERT_EQ(count, 1u);
    EXPECT_EQ(fills[0].order_id, 102u);
    EXPECT_EQ(fills[0].fill_qty, 70u);

    // Order should now be fully filled and inactive
    count = sim.process_market_trade(Side::ASK, 100, 50, 3000, fills, 4);
    EXPECT_EQ(count, 0u);
}

TEST(QueueSimulatorTest, CancelOrderPreventsExecution) {
    QueueFillSimulator<16> sim;
    FillEvent fills[4];

    sim.add_order(103, Side::BID, 100, 100, 0);
    EXPECT_TRUE(sim.cancel_order(103));

    size_t count = sim.process_market_trade(Side::ASK, 100, 100, 1000, fills, 4);
    EXPECT_EQ(count, 0u);
}