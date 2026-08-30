#include <gtest/gtest.h>
#include "latency_model.hpp"

using namespace low_latency;

TEST(LatencyModelTest, CalculatesFixedDelays) {
    LatencyModel model{.wire_delay_ns = 500, .matching_delay_ns = 200};

    constexpr uint64_t SUBMIT_TIME = 1'000'000;

    // Order arrival = 1,000,000 + 500 + 200 = 1,000,700ns
    EXPECT_EQ(model.calculate_order_arrival(SUBMIT_TIME), 1'000'700u);

    // Market data arrival = 1,000,000 + 500 = 1,000,500ns
    EXPECT_EQ(model.calculate_market_data_arrival(SUBMIT_TIME), 1'000'500u);
}