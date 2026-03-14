#pragma once
#include "IWindow.h"
#include "IInputDevice.h"
#include "IHighPrecisionTimer.h"
#include <memory>

namespace SagaEngine::Platform {

struct PlatformObjects {
    std::unique_ptr<IWindow> window;
    std::unique_ptr<IInputDevice> input;
    std::unique_ptr<IHighPrecisionTimer> timer;
    bool valid = false;
};

PlatformObjects CreatePlatformObjects();
void ValidatePlatformObjects(PlatformObjects& objects);

}