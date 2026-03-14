#include <SagaEngine/Core/Time/Time.h>

namespace SagaEngine::Core {

std::chrono::steady_clock::time_point Time::s_Last = std::chrono::steady_clock::now();
float Time::s_Delta = 0.0f;

void Time::Init() {
    s_Last = std::chrono::steady_clock::now();
    s_Delta = 0.0f;
}

void Time::Tick() {
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<float> diff = now - s_Last;
    s_Last = now;
    s_Delta = diff.count();
}

float Time::GetDeltaTime() {
    return s_Delta;
}

double Time::GetTime() {
    auto now = std::chrono::steady_clock::now();
    auto epoch = now.time_since_epoch();
    return std::chrono::duration<double>(epoch).count();
}

}
