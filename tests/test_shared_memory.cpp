#include <gtest/gtest.h>
#include <sys/wait.h>
#include <unistd.h>
#include "shared_memory.hpp"
#include "spsc_queue.hpp"

using TestQueue = SPSCQueue<uint64_t, 1024>;

TEST(SharedMemoryTest, InterProcessCommunication) {
    const std::string shm_name = "/test_spsc_shm";

    pid_t pid = fork();
    ASSERT_NE(pid, -1);

    if (pid == 0) {
        // Child Process: Producer Thread
        usleep(10000); // Wait 10ms for parent to finish mapping & initialization

        SharedMemoryRegion<TestQueue> shm(shm_name, false);
        TestQueue* queue = shm.get();

        for (uint64_t i = 1; i <= 1000; ++i) {
            while (!queue->push(i)) {
                usleep(10);
            }
        }
        exit(0);
    } else {
        // Parent Process: Consumer & Memory Owner
        SharedMemoryRegion<TestQueue> shm(shm_name, true);
        TestQueue* queue = shm.get();

        uint64_t received_count = 0;
        uint64_t val = 0;

        while (received_count < 1000) {
            if (queue->pop(val)) {
                received_count++;
                EXPECT_EQ(val, received_count);
            } else {
                usleep(10);
            }
        }

        int status;
        waitpid(pid, &status, 0);
        EXPECT_TRUE(WIFEXITED(status));
        EXPECT_EQ(WEXITSTATUS(status), 0);
    }
}