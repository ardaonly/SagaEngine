/// @file SDLPlatformFactory.cpp
/// @brief SDL-backed platform factory creation functions.

#include "SagaEngine/Platform/PlatformFactory.h"
#include "SagaEngine/Platform/SDL/SDLDebugRenderer2D.h"
#include "SagaEngine/Platform/SDL/SDLPlatformFactory.h"

#include <memory>

namespace Saga
{

std::unique_ptr<PlatformFactory> CreateSDLPlatformFactory()
{
    return std::make_unique<SDLPlatformFactory>();
}

std::unique_ptr<IDebugRenderer2D> CreateSDLDebugRenderer2D()
{
    return std::make_unique<SDLDebugRenderer2D>();
}

} // namespace Saga
