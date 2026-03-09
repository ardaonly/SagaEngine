#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <cstdint>

namespace SagaEngine::Core {

class JobHandle {
public:
    JobHandle() = default;
    bool IsComplete() const;
    void Wait() const;

private:
    friend class JobSystem;
    std::shared_ptr<std::atomic<int>> _counter;
};

class JobSystem {
public:
    explicit JobSystem(size_t threadCount = 0);
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    template<typename F>
    JobHandle Schedule(F&& job, std::vector<JobHandle> dependencies = {});

    void WaitForAll();
    size_t GetThreadCount() const { return _workers.size(); }
    size_t GetPendingJobCount() const;

private:
    void Enqueue(std::function<void()> job);
    void WorkerThread();

    std::vector<std::thread> _workers;
    std::queue<std::function<void()>> _jobQueue;
    mutable std::mutex _queueMutex;
    std::condition_variable _condition;
    std::atomic<bool> _shutdown{false};
    std::atomic<size_t> _pendingJobs{0};
};

template<typename F>
JobHandle JobSystem::Schedule(F&& job, std::vector<JobHandle> dependencies) {
    auto counter = std::make_shared<std::atomic<int>>(1);

    auto wrappedJob = [func = std::forward<F>(job), counter]() mutable {
        func();
        counter->fetch_sub(1, std::memory_order_release);
    };

    if (!dependencies.empty()) {
        auto depJob = [this, deps = std::move(dependencies), func = std::move(wrappedJob)]() mutable {
            for (const auto& dep : deps) dep.Wait();
            func();
        };
        Enqueue(std::move(depJob));
    } else {
        Enqueue(std::move(wrappedJob));
    }

    JobHandle handle;
    handle._counter = counter;
    return handle;
}

} // namespace SagaEngine::Core