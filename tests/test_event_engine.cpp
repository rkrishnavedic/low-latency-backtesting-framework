#include <gtest/gtest.h>
#include "event_engine.hpp"

using namespace low_latency;

TEST(EventEngineTest, OrdersByTimestamp) {
    EventScheduler scheduler;

    scheduler.schedule(2000, PriorityTier::MARKET_DATA, EventType::MARKET_TICK);
    scheduler.schedule(1000, PriorityTier::MARKET_DATA, EventType::MARKET_TICK);
    scheduler.schedule(3000, PriorityTier::MARKET_DATA, EventType::MARKET_TICK);

    Event ev;
    EXPECT_TRUE(scheduler.pop_next(ev));
    EXPECT_EQ(ev.timestamp_ns, 1000u);

    EXPECT_TRUE(scheduler.pop_next(ev));
    EXPECT_EQ(ev.timestamp_ns, 2000u);

    EXPECT_TRUE(scheduler.pop_next(ev));
    EXPECT_EQ(ev.timestamp_ns, 3000u);

    EXPECT_FALSE(scheduler.pop_next(ev));
}

TEST(EventEngineTest, TieBreaksByPriorityTier) {
    EventScheduler scheduler;
    constexpr uint64_t SAME_TIME = 5000;

    // Schedule same timestamp with different priority tiers
    scheduler.schedule(SAME_TIME, PriorityTier::TIMER, EventType::TIMER_EXPIRE);
    scheduler.schedule(SAME_TIME, PriorityTier::STRATEGY_ORDER, EventType::ORDER_SUBMIT);
    scheduler.schedule(SAME_TIME, PriorityTier::MARKET_DATA, EventType::MARKET_TICK);
    scheduler.schedule(SAME_TIME, PriorityTier::MATCHING_ENGINE, EventType::MATCH_EXECUTION);

    Event ev;
    // Expected order: MARKET_DATA (0) -> MATCHING_ENGINE (1) -> STRATEGY_ORDER (2) -> TIMER (3)
    EXPECT_TRUE(scheduler.pop_next(ev));
    EXPECT_EQ(ev.priority_tier, PriorityTier::MARKET_DATA);

    EXPECT_TRUE(scheduler.pop_next(ev));
    EXPECT_EQ(ev.priority_tier, PriorityTier::MATCHING_ENGINE);

    EXPECT_TRUE(scheduler.pop_next(ev));
    EXPECT_EQ(ev.priority_tier, PriorityTier::STRATEGY_ORDER);

    EXPECT_TRUE(scheduler.pop_next(ev));
    EXPECT_EQ(ev.priority_tier, PriorityTier::TIMER);
}

TEST(EventEngineTest, TieBreaksByFIFOSequenceID) {
    EventScheduler scheduler;
    constexpr uint64_t SAME_TIME = 5000;

    // Schedule same timestamp and priority tier; sequence_id should maintain FIFO order
    scheduler.schedule(SAME_TIME, PriorityTier::MARKET_DATA, EventType::MARKET_TICK); // seq 0
    scheduler.schedule(SAME_TIME, PriorityTier::MARKET_DATA, EventType::MARKET_TICK); // seq 1
    scheduler.schedule(SAME_TIME, PriorityTier::MARKET_DATA, EventType::MARKET_TICK); // seq 2

    Event ev;
    EXPECT_TRUE(scheduler.pop_next(ev));
    EXPECT_EQ(ev.sequence_id, 0u);

    EXPECT_TRUE(scheduler.pop_next(ev));
    EXPECT_EQ(ev.sequence_id, 1u);

    EXPECT_TRUE(scheduler.pop_next(ev));
    EXPECT_EQ(ev.sequence_id, 2u);
}