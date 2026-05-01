#include <gtest/gtest.h>
#include "../src/core/ClockEngine.h"
#include <atomic>
#include <thread>
#include <chrono>

TEST(ClockEngine, TempoClamp) {
    ClockEngine c;
    c.setTempo(5.0);
    EXPECT_GE(c.getTempo(), 20.0);
    c.setTempo(999.0);
    EXPECT_LE(c.getTempo(), 300.0);
}

TEST(ClockEngine, TickFires) {
    ClockEngine c;
    std::atomic<int> ticks{0};
    c.onTick([&](uint64_t) { ++ticks; });
    c.setTempo(6000.0); // very fast for testing
    c.play();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    c.stop();
    EXPECT_GT(ticks.load(), 0);
}

TEST(ClockEngine, QuantizeTick) {
    ClockEngine c;
    // 241 ticks rounded up to nearest 16th (240 ticks) → 480
    EXPECT_EQ(c.quantizeTick(241, 240), 480u);
    EXPECT_EQ(c.quantizeTick(240, 240), 240u);
}
