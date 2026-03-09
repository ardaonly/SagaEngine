#include "SagaEngine/Core/Threading/JobSystem.h"

namespace SagaEngine::Core {

bool JobHandle::IsComplete() const {
    return _counter ? _counter->load(std::memory_order_acquire) == 0 : true;
}

void JobHandle::Wait() const {
    if (_counter) {
        while (!IsComplete()) {
            std::this_thread::yield();
        }
    }
}

JobSystem::JobSystem(size_t threadCount) {
    if (threadCount == 0) {
        threadCount = std::max(1u, std::thread::hardware_concurrency());
    }

    _workers.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i) {
        _workers.emplace_back(&JobSystem::WorkerThread, this);
    }
}

JobSystem::~JobSystem() {
    _shutdown.store(true, std::memory_order_release);
    _condition.notify_all();
    for (auto& worker : _workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void JobSystem::Enqueue(std::function<void()> job) {
    {
        std::unique_lock lock(_queueMutex);
        _jobQueue.push(std::move(job));
        _pendingJobs.fetch_add(1, std::memory_order_relaxed);
    }
    _condition.notify_one();
}

void JobSystem::WorkerThread() {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock lock(_queueMutex);
            _condition.wait(lock, [this] {
                return _shutdown.load(std::memory_order_acquire) || !_jobQueue.empty();
            });

            if (_shutdown.load(std::memory_order_acquire) && _jobQueue.empty()) {
                return;
            }

            job = std::move(_jobQueue.front());
            _jobQueue.pop();
        }

        job();
        _pendingJobs.fetch_sub(1, std::memory_order_relaxed);
    }
}

void JobSystem::WaitForAll() {
    while (_pendingJobs.load(std::memory_order_acquire) > 0) {
        std::this_thread::yield();
    }
}

size_t JobSystem::GetPendingJobCount() const {
    return _pendingJobs.load(std::memory_order_acquire);
}

} // namespace SagaEngine::Core