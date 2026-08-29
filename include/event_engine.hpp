#pragma once

#include <cstdint>
#include <vector>
#include <queue>
#include <functional>
#include "market_event.hpp"

namespace low_latency {

enum class PriorityTier : uint8_t {
    MARKET_DATA = 0, // Exchange ticks & depth updates (highest urgency)
    MATCHING_ENGINE = 1, // Engine internal execution & fill call backs
    STRATEGY_ORDER = 2,// Strategy order requests and cancellations
    TIMER = 3 // periodic timers and timeouts triggers
};

enum class EventType: uint8_t {
    MARKET_TICK,
    ORDER_SUBMIT,
    ORDER_CANCEL,
    MATCH_EXECUTION,
    TIMER_EXPIRE
};

struct Event {
    MarketEvent market_event{};
    uint64_t timestamp_ns{0};
    uint64_t sequence_id{0};
    PriorityTier priority_tier{PriorityTier::MARKET_DATA};
    EventType event_type{EventType::MARKET_TICK};
};

// min heap comparator: smallest timestamp/tier/sequence at the top
struct EventComparator {
    bool operator()(const Event& a, const Event&b) const noexcept {
        if(a.timestamp_ns != b.timestamp_ns){
            return a.timestamp_ns > b.timestamp_ns;
        }
        if (a.priority_tier != b.priority_tier) {
            return static_cast<uint8_t>(a.priority_tier) > static_cast<uint8_t>(b.priority_tier);
        }
        return a.sequence_id > b.sequence_id;
    }
};

class EventScheduler {
    std::vector<Event> container_;
    std::priority_queue<Event, std::vector<Event>, EventComparator> queue_{EventComparator(), std::move(container_)};
    uint64_t next_sequence_id_{0};
    uint64_t current_time_ns_{0};
public:
    explicit EventScheduler(size_t reserve_capacity = 100'000) {
        container_.reserve(reserve_capacity);
    }

    void schedule(uint64_t timestamp_ns, PriorityTier priority, EventType event_type, const MarketEvent& payload = {}) {
          Event ev{
            .timestamp_ns = timestamp_ns,
            .priority_tier = priority,
            .sequence_id = next_sequence_id_++,
            .event_type = event_type,
            .market_event = payload
          };
          queue_.push(ev);
    }

    [[nodiscard]] bool empty() const noexcept {
        return queue_.empty();
    }

    [[nodiscard]] size_t size() const noexcept {
        return queue_.size();
    }

    [[nodiscard]] uint64_t current_time_ns() const noexcept {
        return current_time_ns_;
    }

    bool pop_next(Event& out_event) {
        if (queue_.empty()) return false;

        out_event = queue_.top();
        queue_.pop();
        
        current_time_ns_ = out_event.timestamp_ns;
        return true;
    }

    void reset() noexcept {
        while(!queue_.empty()) queue_.pop();
        next_sequence_id_ = 0;
        current_time_ns_ = 0;
    }
};

} // namespace low_latency