#include "SagaEngine/Platform/Windows/HighPrecisionTimerWin32.h"
#include <Windows.h>

namespace SagaEngine::Platform {

HighPrecisionTimerWin32::HighPrecisionTimerWin32() {
    LARGE_INTEGER freq;
    if (QueryPerformanceFrequency(&freq)) {
        qpcAvailable = true;
        frequency = freq.QuadPart;
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        startCount = lastCount = now.QuadPart;
    } else {
        qpcAvailable = false;
        steadyStart = steadyLast = std::chrono::steady_clock::now();
    }
}

HighPrecisionTimerWin32::~HighPrecisionTimerWin32() = default;

void HighPrecisionTimerWin32::Reset() {
    if (qpcAvailable) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        startCount = lastCount = now.QuadPart;
    } else {
        steadyStart = steadyLast = std::chrono::steady_clock::now();
    }
}

long long HighPrecisionTimerWin32::GetQPC() const noexcept {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart;
}

double HighPrecisionTimerWin32::GetTimeSeconds() const {
    if (qpcAvailable) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return double(now.QuadPart - startCount) / double(frequency);
    } else {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> d = now - steadyStart;
        return d.count();
    }
}

double HighPrecisionTimerWin32::GetDeltaSeconds() {
    if (qpcAvailable) {
        long long now = GetQPC();
        double delta = double(now - lastCount) / double(frequency);
        lastCount = now;
        return delta;
    } else {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> d = now - steadyLast;
        steadyLast = now;
        return d.count();
    }
}

} // namespace SagaEngine::Platform