#pragma once

#include <cstdint>

namespace low_latency {

struct LatencyModel {
    uint64_t wire_delay_ns{500};      // One-way network delay (default 500ns)
    uint64_t matching_delay_ns{200};  // Exchange processing delay (default 200ns)

    // Timestamp when an order reaches the exchange
    [[nodiscard]] constexpr uint64_t calculate_order_arrival(uint64_t submission_time_ns) const noexcept {
        return submission_time_ns + wire_delay_ns + matching_delay_ns;
    }

    // Timestamp when market data reaches the strategy engine
    [[nodiscard]] constexpr uint64_t calculate_market_data_arrival(uint64_t exchange_time_ns) const noexcept {
        return exchange_time_ns + wire_delay_ns;
    }
};

} // namespace low_latency