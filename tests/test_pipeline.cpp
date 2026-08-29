#include <gtest/gtest.h>
#include <sys/wait.h>
#include <unistd.h>
#include <x86intrin.h>
#include <vector>

#include "shared_memory.hpp"
#include "spsc_queue.hpp"
#include "order_book.hpp"
#include "market_event.hpp"
#include "rdtsc.hpp"

using MarketDataQueue = SPSCQueue<MarketEvent, 4096>;

TEST(PipelineTest, EndToEndGatewayToMatchingEngine) {
    const std::string shm_name = "/hft_pipeline_shm";
    constexpr size_t TOTAL_EVENTS = 50'000;

    pid_t pid = fork();
    ASSERT_NE(pid, -1);

    if (pid == 0) {
        // -------------------------------------------------------------------
        // CHILD PROCESS: Market Data Gateway (Producer)
        // -------------------------------------------------------------------
        usleep(20000); // Allow parent to construct shared memory queue

        {
            SharedMemoryRegion<MarketDataQueue> shm(shm_name, false);
            MarketDataQueue* queue = shm.get();

            std::vector<uint32_t> inserted_indices;
            inserted_indices.reserve(TOTAL_EVENTS);

            // Stream 50,000 ADD order events
            for (uint64_t i = 0; i < TOTAL_EVENTS; ++i) {
                MarketEvent event{
                    .timestamp = profiling::rdtsc_start(),
                    .order_id = 100000 + i,
                    .price = static_cast<uint32_t>(5000 + (i % 100)),
                    .qty = 10,
                    .order_idx = INVALID_INDEX,
                    .side = (i % 2 == 0) ? Side::BID : Side::ASK,
                    .type = MarketEventType::ADD
                };

                while (!queue->push(event)) {
                    _mm_pause(); // Low-latency spin hint
                }
            }
        }
        std::_Exit(0);
    } else {
        // -------------------------------------------------------------------
        // PARENT PROCESS: Matching Engine (Consumer)
        // -------------------------------------------------------------------
        SharedMemoryRegion<MarketDataQueue> shm(shm_name, true);
        MarketDataQueue* queue = shm.get();
        FlatOrderBook order_book;

        size_t processed_count = 0;
        MarketEvent event;
        std::vector<uint64_t> latencies_cycles;
        latencies_cycles.reserve(TOTAL_EVENTS);

        while (processed_count < TOTAL_EVENTS) {
            if (queue->pop(event)) {
                uint64_t arrival_time = profiling::rdtsc_end();

                if (event.type == MarketEventType::ADD) {
                    uint32_t handle = order_book.add_order(
                        event.order_id, event.price, event.qty, event.side
                    );
                    EXPECT_NE(handle, INVALID_INDEX);
                }

                latencies_cycles.push_back(arrival_time - event.timestamp);
                processed_count++;
            } else {
                _mm_pause(); // Low-latency spin hint
            }
        }

        int status;
        waitpid(pid, &status, 0);
        EXPECT_TRUE(WIFEXITED(status));
        EXPECT_EQ(WEXITSTATUS(status), 0);

        // Verify state consistency
        EXPECT_EQ(processed_count, TOTAL_EVENTS);
    }
}