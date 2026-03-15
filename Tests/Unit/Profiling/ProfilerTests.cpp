// Tests/Unit/Profiling/ProfilerTests.cpp
#include "SagaEngine/Core/Profiling/Profiler.h"
#include <gtest/gtest.h>
#include <thread>

using namespace SagaEngine::Core;

class ProfilerTest : public ::testing::Test {
protected:
    void SetUp() override {
        Profiler::Instance().Clear();
    }
};

TEST_F(ProfilerTest, Singleton) {
    auto& p1 = Profiler::Instance();
    auto& p2 = Profiler::Instance();
    EXPECT_EQ(&p1, &p2);
}

TEST_F(ProfilerTest, SampleLifecycle) {
    auto& profiler = Profiler::Instance();
    
    profiler.BeginSample("TestScope");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    profiler.EndSample("TestScope");
    
    auto& stats = profiler.GetStats("TestScope");
    
    EXPECT_GE(stats.callCount.load(), 1);
    EXPECT_GE(stats.totalTimeNs.load(), 1'000'000);
}

TEST_F(ProfilerTest, ThreadSafety) {
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