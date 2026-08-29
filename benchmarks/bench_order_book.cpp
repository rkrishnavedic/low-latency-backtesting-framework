#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <iomanip>

#include "order_book.hpp"
#include "rdtsc.hpp"

constexpr size_t NUM_ITERATIONS = 100'000;
constexpr size_t WARMUP_RUNS = 10'000;

void print_stats(const std::string& name, std::vector<uint64_t>& cycles) {
    std::sort(cycles.begin(), cycles.end());

    uint64_t p50 = cycles[static_cast<size_t>(NUM_ITERATIONS*0.50)];
    uint64_t p90 = cycles[static_cast<size_t>(NUM_ITERATIONS*0.90)];
    uint64_t p99 = cycles[static_cast<size_t>(NUM_ITERATIONS*0.99)];
    uint64_t p999 = cycles[static_cast<size_t>(NUM_ITERATIONS*0.999)];
    uint64_t max = cycles.back();

    std::cout << "========================================\n";
    std::cout << " Benchmark Results: " << name << "\n";
    std::cout << "========================================\n";
    std::cout << " p50   (Median) : " << std::setw(6) << p50  << " cycles\n";
    std::cout << " p90            : " << std::setw(6) << p90  << " cycles\n";
    std::cout << " p99            : " << std::setw(6) << p99  << " cycles\n";
    std::cout << " p99.9          : " << std::setw(6) << p999 << " cycles\n";
    std::cout << " Max (Tail Jitter): " << std::setw(4) << max  << " cycles\n";
    std::cout << "----------------------------------------\n\n";
}

int main() {
    FlatOrderBook book;
    std::vector<uint64_t> add_cycles;
    std::vector<uint64_t> cancel_cycles;
    std::vector<uint32_t> handles;

    add_cycles.reserve(NUM_ITERATIONS);
    cancel_cycles.reserve(NUM_ITERATIONS);
    handles.reserve(NUM_ITERATIONS);

    // 1. Warmup Phase (Populate L1 Instruction and Data Caches)
    for (size_t i = 0; i < WARMUP_RUNS; ++i) {
        uint32_t h = book.add_order(i, 5000 + (i % 100), 10, Side::BID);
        book.cancel_order(h);
    }

    // 2. Benchmark Order Insertions
    for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
        uint64_t start = profiling::rdtsc_start();
        uint32_t handle = book.add_order(100000 + i, 5000 + (i % 50), 10, Side::BID);
        uint64_t end = profiling::rdtsc_end();

        add_cycles.push_back(end - start);
        handles.push_back(handle);
    }

    // 3. Benchmark Order Cancellations
    for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
        uint64_t start = profiling::rdtsc_start();
        book.cancel_order(handles[i]);
        uint64_t end = profiling::rdtsc_end();

        cancel_cycles.push_back(end - start);
    }

    print_stats("Order Insertion (add_order)", add_cycles);
    print_stats("Order Cancellation (cancel_order)", cancel_cycles);

    return 0;
}