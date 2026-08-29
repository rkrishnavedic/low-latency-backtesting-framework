#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <utility>
#include <new>

#include "common.hpp"

template <typename T, size_t Capacity>
class alignas(CACHE_LINE_SIZE) ArenaPool {
    alignas(CACHE_LINE_SIZE) std::array<T, Capacity> storage_;
    alignas(CACHE_LINE_SIZE) std::array<uint32_t, Capacity> free_list_;
    uint32_t free_head_{0};
public:
    ArenaPool(){
        // Initialize free list linked via array indices
        for (size_t i=0; i<Capacity-1;++i){
            free_list_[i] = static_cast<uint32_t>(i+1);
        }
        free_list_[Capacity-1] = INVALID_INDEX;
        free_head_ = 0;
    }

    // Allocate element in-place using placement new
    template <typename... Args>
    uint32_t allocate(Args&&... args){
        if (free_head_ == INVALID_INDEX) [[unlikely]] {
            return INVALID_INDEX; // Capacity exhausted
        }
        
        uint32_t idx = free_head_;
        free_head_ = free_list_[idx];

        // Placement new to construct object in pre-allocated memory
        new (&storage_[idx]) T(std::forward<Args>(args)...);
        return idx;
    }

    // Return element to free list
    void deallocate(uint32_t idx){
        assert(idx<Capacity);
        storage_[idx].~T(); // Explicit destructor call
        free_list_[idx] = free_head_;
        free_head_ = idx;
    }

    // O(1) Index Accessors
    T& operator[](uint32_t idx) { return storage_[idx]; }
    const T& operator[](uint32_t idx) const { return storage_[idx]; }

    [[nodiscard]] size_t capacity() const { return Capacity; }
    [[nodiscard]] uint32_t free_head() const { return free_head_; }
};