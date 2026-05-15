#pragma once

#include <atomic>
#include <thread>

namespace SagaEngine::Core {

class SpinLock {
public:
    void lock() {
        while (_flag.test_and_set(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }

    void unlock() {
        _flag.clear(std::memory_order_release);
    }

    bool try_lock() {
        return !_flag.test_and_set(std::memory_order_acquire);
    }

private:
    std::atomic_flag _flag = ATOMIC_FLAG_INIT;
};

class SpinLockGuard {
public:
    explicit SpinLockGuard(SpinLock& lock) : _lock(lock) { _lock.lock(); }
    ~SpinLockGuard() { _lock.unlock(); }

    SpinLockGuard(const SpinLockGuard&) = delete;
    SpinLockGuard& operator=(const SpinLockGuard&) = delete;

private:
    SpinLock& _lock;
};

} // namespace SagaEngine::Core