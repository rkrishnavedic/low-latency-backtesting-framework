#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <type_traits>

#include "common.hpp"

template <typename T, size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity-1)) == 0, "Capacity must be a power of 2");

    // Padded to distinct 64-byte cache lines to eliminate False Sharaing
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};
    alignas(CACHE_LINE_SIZE) std::array<T, Capacity> buffer_;

public:
    SPSCQueue() : head_(0), tail_(0) {}

    // Lock-free Push (Producer Thread Only)
    template <typename... Args>
    bool push(Args&&... args) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t current_head = head_.load(std::memory_order_acquire);

        if (current_tail - current_head>=Capacity) {
            return false; // Queue Full
        }

        const size_t idx = current_tail & (Capacity-1);
        buffer_[idx] = T(std::forward<Args>(args)...);

        // Publish updated tail to consumer with release semantics
        tail_.store(current_tail+1, std::memory_order_release);
        return true;
    }

    // Lock-free Pop (Consumer Thread Only)
    bool pop(T& item) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        const size_t current_tail = tail_.load(std::memory_order_acquire);

        if (current_head == current_tail) {
            return false; // Queue empty
        }

        const size_t idx = current_head & (Capacity-1);
        item = std::move(buffer_[idx]);

        // Publish updated head to producer with release semantics
        head_.store(current_head+1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const {
        return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] size_t size() const {
        const size_t t = tail_.load(std::memory_order_relaxed);
        const size_t h = head_.load(std::memory_order_relaxed);
        return (t >= h) ? (t - h) : 0;
    }
};