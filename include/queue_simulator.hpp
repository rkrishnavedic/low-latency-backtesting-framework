#pragma once

#include <cstdint>
#include <array>
#include <algorithm>
#include "market_event.hpp"

namespace low_latency {

struct FillEvent {
    uint64_t timestamp_ns{0};
    uint64_t order_id{0};
    uint32_t fill_qty{0};
    uint32_t price{0};
    Side side{Side::BID};
};

struct SimulatedOrder {
    uint64_t order_id{0};
    uint32_t price{0};
    uint32_t qty{0};
    uint32_t filled_qty{0};
    uint32_t volume_ahead{0};
    Side side{Side::BID};
    bool active{false};

    [[nodiscard]] uint32_t remaining_qty() const noexcept {
        return qty - filled_qty;
    }
};

template<size_t MaxOrders=1024>
class QueueFillSimulator {
    std::array<SimulatedOrder, MaxOrders> orders_{};
public:
    QueueFillSimulator() = default;

    // register a strategy limit order with the current market volume sitting ahead at that price level
    bool add_order(uint64_t order_id, Side side, uint32_t price, uint32_t qty, uint32_t volume_ahead_at_level) noexcept {
        for (auto& slot : orders_) {
            if (!slot.active) {
                slot = SimulatedOrder{
                    .order_id = order_id,
                    .price = price,
                    .qty = qty,
                    .filled_qty = 0,
                    .volume_ahead = volume_ahead_at_level,
                    .side = side,
                    .active = true
                };
                return true;
            }
        }
        return false; // Storage capacity reached
    }

    bool cancel_order(uint64_t order_id) noexcept {
        for (auto& slot: orders_) {
            if(slot.active && slot.order_id == order_id){
                slot.active = false;
                return true;
            }
        }
        return false;
    }

    // process market trade tick and calculate fills for strategy orders
    size_t process_market_trade(
        Side trade_side, uint32_t trade_price, uint32_t trade_qty,
        uint64_t timestamp_ns, FillEvent* out_fills, size_t max_fills
    ) noexcept {
        size_t num_fills = 0;

        for(auto& slot: orders_){
            if (!slot.active) continue;

            /*
            match conditions
                - an incoming ASK trade (sell market order) aggresses against bid limit order (slot.price>=trade_price)
                - an incoming BID trade (buy market order) aggresses against ask limit order (slot.price<=trade_price)
            */
            bool applies = false;
            if (slot.side == Side::BID && trade_side == Side::ASK && slot.price >= trade_price) {
                applies = true;
            } else if (slot.side == Side::ASK && trade_side == Side::BID && slot.price <= trade_price) {
                applies = true;
            }

            if (!applies) continue;

            uint32_t remaining_trade_vol = trade_qty;

            // 1. consume volume sitting ahead in queue first
            if (slot.volume_ahead>0){
                uint32_t consume_ahead = std::min(slot.volume_ahead, remaining_trade_vol);
                slot.volume_ahead -= consume_ahead;
                remaining_trade_vol -= consume_ahead;
            }

            // 2. fill strategy order once queue ahead is cleared
            if (slot.volume_ahead == 0 && remaining_trade_vol >0 && num_fills < max_fills) {
                uint32_t fill_qty = std::min(slot.remaining_qty(), remaining_trade_vol);
                slot.filled_qty += fill_qty;

                out_fills[num_fills++] = FillEvent{
                    .order_id = slot.order_id,
                    .fill_qty = fill_qty,
                    .price = slot.price,
                    .side = slot.side,
                    .timestamp_ns = timestamp_ns
                };

                if (slot.remaining_qty() == 0){
                    slot.active = false;
                }
            }
        }
        return num_fills;
    }

    void reset() noexcept {
        for (auto& slot: orders_){
            slot.active = false;
        }
    }

};

} // namespace low_latency