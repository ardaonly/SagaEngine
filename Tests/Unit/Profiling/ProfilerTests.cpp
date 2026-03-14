// Tests/Unit/Profiling/ProfilerTests.cpp
#include "SagaEngine/Core/Profiling/Profiler.h"
#include <gtest/gtest.h>
#include <thread>

using namespace SagaEngine::Core;

TEST(ProfilerTest, Singleton) {
    auto& p1 = Profiler::Instance();
    auto& p2 = Profiler::Instance();
    EXPECT_EQ(&p1, &p2);
}

TEST(ProfilerTest, SampleLifecycle) {
    auto& profiler = Profiler::Instance();
    
    profiler.BeginSample("TestScope");
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    profiler.EndSample("TestScope");
    
    auto& stats = profiler.GetStats("TestScope");
    EXPECT_GE(stats.callCount.load(), 1);
    EXPECT_GE(stats.totalTimeNs.load(), 100'000);  // 100us in ns
}

TEST(ProfilerTest, ThreadSafety) {
    auto& profiler = Profiler::Instance();
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&profiler]() {
            for (int j = 0; j < 100; ++j) {
                profiler.BeginSample("Concurrent");
                profiler.EndSample("Concurrent");
            }
        });
    }
    
    for (auto& t : threads) t.join();
    
    auto& stats = profiler.GetStats("Concurrent");
    EXPECT_EQ(stats.callCount.load(), 400);
}