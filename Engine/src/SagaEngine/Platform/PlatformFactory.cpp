#include "SagaEngine/Platform/PlatformFactory.h"
#include "SagaEngine/Core/Log/Log.h"
#include <cstdio>

#if defined(_WIN32)
#include "SagaEngine/Platform/Windows/WindowWin32.h"
#include "SagaEngine/Platform/Windows/InputWin32.h"
#include "SagaEngine/Platform/Windows/HighPrecisionTimerWin32.h"
#endif

namespace SagaEngine::Platform {

PlatformObjects CreatePlatformObjects() {
    std::printf("[PlatformFactory] Creating platform objects...\n");
    std::fflush(stdout);

    PlatformObjects objects;

#if defined(_WIN32)
    try {
        std::printf("[PlatformFactory] Windows platform detected\n");
        std::fflush(stdout);

        objects.window = std::make_unique<WindowWin32>();
        std::printf("[PlatformFactory] Window created: %p\n", objects.window.get());
        std::fflush(stdout);

        objects.input = std::make_unique<InputWin32>();
        std::printf("[PlatformFactory] Input created: %p\n", objects.input.get());
        std::fflush(stdout);

        objects.timer = std::make_unique<HighPrecisionTimerWin32>();
        std::printf("[PlatformFactory] Timer created: %p\n", objects.timer.get());
        std::fflush(stdout);

        objects.valid = true;
        std::printf("[PlatformFactory] Platform objects created successfully\n");
        std::fflush(stdout);
    }
    catch (const std::exception& e) {
        std::printf("[PlatformFactory] EXCEPTION: %s\n", e.what());
        std::fflush(stdout);
        objects.valid = false;
    }
    catch (...) {
        std::printf("[PlatformFactory] UNKNOWN EXCEPTION\n");
        std::fflush(stdout);
        objects.valid = false;
    }
#else
    std::printf("[PlatformFactory] ERROR: No platform implementation\n");
    std::fflush(stdout);
    objects.valid = false;
#endif

    return objects;
}

void ValidatePlatformObjects(PlatformObjects& objects) {
    if (!objects.valid) {
        throw std::runtime_error("Platform objects are not valid");
    }

    if (!objects.window) {
        throw std::runtime_error("Window object is null");
    }

    std::printf("[PlatformFactory] Platform validation passed\n");
    std::fflush(stdout);
}

}