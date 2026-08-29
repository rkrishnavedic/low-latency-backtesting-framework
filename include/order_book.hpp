#pragma once

#include <array>
#include <cstdint>
#include <cassert>

#include "common.hpp"
#include "arena_pool.hpp"

constexpr size_t MAX_ORDERS = 100'000;
// TODO(Architecture): 10,000 levels is small for wide-spread assets. 
// If asset prices fluctuate beyond this, modulo indexing will collide.
constexpr size_t MAX_PRICE_LEVELS = 10'000;

enum class Side : uint8_t {
    BID = 0,
    ASK = 1
};

struct alignas(CACHE_LINE_SIZE) Order {
    uint64_t order_id{0};
    uint32_t price{0};
    uint32_t qty{0};
    uint32_t next_order_idx{INVALID_INDEX};
    uint32_t prev_order_idx{INVALID_INDEX};
    Side side{Side::BID};
};

struct alignas(CACHE_LINE_SIZE) LimitLevel {
    uint32_t price{0};
    uint32_t total_volume{0};
    uint32_t order_count{0};
    uint32_t head_order_idx{INVALID_INDEX};
    uint32_t tail_order_idx{INVALID_INDEX};
};

class FlatOrderBook {
    ArenaPool<Order, MAX_ORDERS> order_pool_;
    alignas(CACHE_LINE_SIZE) std::array<LimitLevel, MAX_PRICE_LEVELS> bid_levels_;
    alignas(CACHE_LINE_SIZE) std::array<LimitLevel, MAX_PRICE_LEVELS> ask_levels_;
    // TODO(Feature): Add Best Bid / Best Ask state tracking.
    // HFT Standard: Use a `std::bitset<MAX_PRICE_LEVELS> active_bids_` and 
    // `__builtin_clz` / `__builtin_ctz` to instantly find the next populated price level when a level is cleared.
public:
    FlatOrderBook() {
        bid_levels_.fill(LimitLevel{});
        ask_levels_.fill(LimitLevel{});
    }

    // Add Limit Order to Book (O(1))
    [[nodiscard]] uint32_t add_order(uint64_t order_id, uint32_t price, uint32_t qty, Side side){
        uint32_t order_idx = order_pool_.allocate(
            Order{
                .order_id = order_id,
                .price = price,
                .qty = qty,
                .next_order_idx = INVALID_INDEX,
                .prev_order_idx = INVALID_INDEX,
                .side = side
            }
        );

        if (order_idx == INVALID_INDEX) [[unlikely]] {
            return INVALID_INDEX; //Arena pool full
        }

        uint32_t level_idx = price % MAX_PRICE_LEVELS;
        LimitLevel& level = (side == Side::BID) ? bid_levels_[level_idx] : ask_levels_[level_idx];

        if (level.head_order_idx == INVALID_INDEX){
            level.price = price;
            level.total_volume = qty;
            level.order_count = 1;
            level.head_order_idx = order_idx;
            level.tail_order_idx = order_idx;
        } else {
            level.total_volume += qty;
            level.order_count++;

            // Append to tail of the doubly-linked index queue
            Order& new_order = order_pool_[order_idx];
            new_order.prev_order_idx = level.tail_order_idx;
            order_pool_[level.tail_order_idx].next_order_idx = order_idx;
            level.tail_order_idx = order_idx;
        }

        return order_idx;
    }

    // Cancel Order from Book (O(1))
    bool cancel_order(uint32_t order_idx){
        if (order_idx>=MAX_ORDERS) [[unlikely]] return false;

        Order& order = order_pool_[order_idx];
        uint32_t level_idx = order.price % MAX_PRICE_LEVELS;
        LimitLevel& level = (order.side == Side::BID)? bid_levels_[level_idx] : ask_levels_[level_idx];

        if (level.total_volume < order.qty) return false;
        
        level.total_volume -= order.qty;
        level.order_count--;

        // Unlink from doubly-linked ring
        // Detach from prev end order
        if (order.prev_order_idx != INVALID_INDEX) {
            // not at top of order book
            order_pool_[order.prev_order_idx].next_order_idx = order.next_order_idx;
        } else {
            level.head_order_idx = order.next_order_idx;
        }

        // Detach from next end order
        if (order.next_order_idx != INVALID_INDEX) {
            order_pool_[order.next_order_idx].prev_order_idx = order.prev_order_idx;
        } else {
            level.tail_order_idx = order.prev_order_idx;
        }

        order_pool_.deallocate(order_idx);
        return true;
    }

    // Accessors
    [[nodiscard]] const LimitLevel& get_level(uint32_t price, Side side) const {
        uint32_t level_idx = price % MAX_PRICE_LEVELS;
        return (side==Side::BID)? bid_levels_[level_idx] : ask_levels_[level_idx];
    }

    [[nodiscard]] const Order& get_order(uint32_t order_idx) const {
        return order_pool_[order_idx];
    }
};