#pragma once

#include <cstdint>
#include "common.hpp"
#include "order_book.hpp"

enum class MarketEventType : uint8_t {
    ADD = 0,
    CANCEL = 1
};

struct alignas(CACHE_LINE_SIZE) MarketEvent {
    uint64_t timestamp_ns{0};
    uint64_t order_id{0};
    uint32_t price{0};
    uint32_t qty{0};
    uint32_t order_idx{INVALID_INDEX}; // Set for CANCEL events
    Side side{Side::BID};
    MarketEventType type{MarketEventType::ADD};
};