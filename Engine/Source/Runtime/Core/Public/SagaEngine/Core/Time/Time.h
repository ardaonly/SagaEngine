#pragma once

#include <chrono>

namespace SagaEngine::Core {

class Time {
public:
    static void Init();
    static void Tick();
    static float GetDeltaTime();
    static double GetTime();
private:
    static std::chrono::steady_clock::time_point s_Last;
    static float s_Delta;
};

}
