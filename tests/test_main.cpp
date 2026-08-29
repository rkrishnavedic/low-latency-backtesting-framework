#include <gtest/gtest.h>
#include "common.hpp"

TEST(EnvironmentTest, CacheLineAlignment) {
    EXPECT_EQ(CACHE_LINE_SIZE, 64);
}