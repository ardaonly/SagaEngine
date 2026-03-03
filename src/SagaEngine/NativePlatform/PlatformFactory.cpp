#include <SagaEngine/NativePlatform/PlatformFactory.h>

// platform selection logic - currently Windows only
#if defined(_WIN32)
#include "SagaEngine/NativePlatform/Windows/WindowWin32.h"
#include "SagaEngine/NativePlatform/Windows/InputWin32.h"
#include "SagaEngine/NativePlatform/Windows/HighPrecisionTimerWin32.h"
#else
#error "PlatformFactory: No platform implementation for this OS"
#endif

namespace SagaEngine::NativePlatform
{

PlatformObjects CreatePlatformObjects()
{
    PlatformObjects o;
#if defined(_WIN32)
    auto window = std::make_unique<WindowWin32>();
    auto input = std::make_unique<InputWin32>();
    auto timer = std::make_unique<HighPrecisionTimerWin32>();

    o.window = std::move(window);
    o.input = std::move(input);
    o.timer = std::move(timer);
#endif
    return o;
}
}
