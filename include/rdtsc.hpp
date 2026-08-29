#pragma once

#include <cstdint>
#include <x86intrin.h>

namespace profiling {
    // Serializing start timestamp: forces pipeline to drain prior instructions
    [[nodiscard]] inline uint64_t rdtsc_start() {
        __builtin_ia32_lfence();
        return __builtin_ia32_rdtsc();
    }

    [[nodiscard]] inline uint64_t rdtsc_end() {
        unsigned int aux;
        uint64_t tsc = __rdtscp(&aux);
        __builtin_ia32_lfence();
        return tsc;
    }
} // namespace profiling