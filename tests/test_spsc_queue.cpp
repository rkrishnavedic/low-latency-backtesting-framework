#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <numeric>

#include "spsc_queue.hpp"

struct MarketEvent {
    uint64_t timestamp;
    uint32_t price;
    uint32_t qty;
};

TEST(SPSCQueueTest, BasicPushPop) {
    SPSCQueue<MarketEvent, 1024> queue;

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);

    bool pushed = queue.push(MarketEvent{10001, 50000, 10});
    EXPECT_TRUE(pushed);
    EXPECT_FALSE(queue.empty());
    EXPECT_EQ(queue.size(), 1);

    MarketEvent event;
    bool popped = queue.pop(event);
    EXPECT_TRUE(popped);
    EXPECT_EQ(event.timestamp, 10001);
    EXPECT_EQ(event.price, 50000);
    EXPECT_TRUE(queue.empty());
}

TEST(SPSCQueueTest, FullQueueWrapAround) {
    SPSCQueue<uint32_t, 4> queue;

    EXPECT_TRUE(queue.push(10));
    EXPECT_TRUE(queue.push(20));
    EXPECT_TRUE(queue.push(30));
    EXPECT_TRUE(queue.push(40));

    // 5th push must fail (capacity 4 exhausted)
    EXPECT_FALSE(queue.push(50));

    uint32_t val;
    EXPECT_TRUE(queue.pop(val));
    EXPECT_EQ(val, 10);

    // Slot freed, push should succeed
    EXPECT_TRUE(queue.push(50));
}

TEST(SPSCQueueTest, ConcurrentProducerConsumer) {
    constexpr size_t NUM_ITEMS = 1'000'000;
    SPSCQueue<size_t, 4096> queue;

    std::thread producer([&]() {
        for (size_t i = 0; i < NUM_ITEMS; ++i) {
            while (!queue.push(i)) {
                // Spin until space is free
                std::this_thread::yield();
            }
        }
    });

    std::vector<size_t> received;
    received.reserve(NUM_ITEMS);

    std::thread consumer([&]() {
        size_t val = 0;
        for (size_t i = 0; i < NUM_ITEMS; ++i) {
            while (!queue.pop(val)) {
                // Spin until item is published
                std::this_thread::yield();
            }
            received.push_back(val);
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(received.size(), NUM_ITEMS);
    for (size_t i = 0; i < NUM_ITEMS; ++i) {
        ASSERT_EQ(received[i], i);
    }
}