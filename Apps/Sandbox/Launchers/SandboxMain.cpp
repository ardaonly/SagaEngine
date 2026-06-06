/// @file SandboxMain.cpp
/// @brief Main entry point for the windowed Sandbox application.

#ifndef SDL_MAIN_HANDLED
#   define SDL_MAIN_HANDLED
#endif

#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Core/Memory/MemoryTracker.h>
#include <SagaEngine/Platform/PlatformFactory.h>
#include <SagaSandbox/Core/SandboxHost.h>
#include <SagaSandbox/Core/SandboxConfig.h>

#include <SDL2/SDL.h>

#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#if defined(__linux__)
#   include <dlfcn.h>
#endif

namespace
{

void* SagaSandboxTrackedAlloc(std::size_t size)
{
    if (size == 0)
        size = 1;

    void* ptr = std::malloc(size);
    if (!ptr)
        throw std::bad_alloc();

    SagaEngine::Core::MemoryTracker::Instance().TrackAllocation(
        ptr, size, "global operator new", 0, "SandboxGlobalNew");
    return ptr;
}

void SagaSandboxTrackedFree(void* ptr) noexcept
{
    if (!ptr)
        return;

    SagaEngine::Core::MemoryTracker::Instance().TrackDeallocation(ptr);
    std::free(ptr);
}

bool ArgEquals(const char* arg, const char* literal) noexcept
{
    return std::strcmp(arg, literal) == 0;
}

bool HasEnv(const char* name) noexcept
{
    const char* value = std::getenv(name);
    return value && value[0] != '\0';
}

void ConfigureLinuxVideoDriver(bool forceX11, bool nativeWayland)
{
#if defined(__linux__)
    if (nativeWayland)
        return;

    if (!forceX11 && !HasEnv("WAYLAND_DISPLAY"))
        return;

    const char* currentSDLDriver = std::getenv("SDL_VIDEODRIVER");
    if (currentSDLDriver && currentSDLDriver[0] != '\0' &&
        !forceX11 && std::strcmp(currentSDLDriver, "wayland") != 0)
    {
        LOG_INFO("SandboxMain", "Keeping user-provided SDL_VIDEODRIVER=%s.", currentSDLDriver);
        return;
    }

    if (!HasEnv("DISPLAY"))
    {
        LOG_WARN("SandboxMain",
                 "Wayland session detected but DISPLAY is not set; native Wayland will run without Diligent GPU presentation.");
        return;
    }

    SDL_SetHint("SDL_VIDEODRIVER", "x11");
    setenv("SDL_VIDEODRIVER", "x11", 1);
    LOG_INFO("SandboxMain",
             "Using SDL_VIDEODRIVER=x11 for Diligent GPU presentation under Wayland/XWayland. Pass --native-wayland to disable this fallback.");
#else
    (void)forceX11;
    (void)nativeWayland;
#endif
}

void ProbeVulkanLoader()
{
#if defined(__linux__)
    static void* loader = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_GLOBAL);
    if (!loader)
    {
        const char* err = dlerror();
        LOG_WARN("SandboxMain",
                 "libvulkan.so.1 could not be loaded before Diligent init: %s. On NixOS, re-enter shell.nix so LD_LIBRARY_PATH includes vulkan-loader/lib.",
                 err ? err : "unknown error");
    }
    else
    {
        LOG_INFO("SandboxMain", "Vulkan loader preloaded: libvulkan.so.1");
    }
#endif
}

} // namespace

void* operator new(std::size_t size)
{
    return SagaSandboxTrackedAlloc(size);
}

void* operator new[](std::size_t size)
{
    return SagaSandboxTrackedAlloc(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    try { return SagaSandboxTrackedAlloc(size); }
    catch (...) { return nullptr; }
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    try { return SagaSandboxTrackedAlloc(size); }
    catch (...) { return nullptr; }
}

void operator delete(void* ptr) noexcept
{
    SagaSandboxTrackedFree(ptr);
}

void operator delete[](void* ptr) noexcept
{
    SagaSandboxTrackedFree(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept
{
    SagaSandboxTrackedFree(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept
{
    SagaSandboxTrackedFree(ptr);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept
{
    SagaSandboxTrackedFree(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept
{
    SagaSandboxTrackedFree(ptr);
}

int main(int argc, char* argv[])
{
    SagaEngine::Core::Log::Init();

    auto platformFactory = Saga::CreateSDLPlatformFactory();
    Saga::PlatformFactory::Set(platformFactory.get());

    SagaSandbox::SandboxConfig config;
    config.windowTitle       = "SagaEngine Sandbox";
    config.initialScenarioId = "render_playground";

    bool forceX11       = false;
    bool nativeWayland  = false;

    // Parse command line: [scenario_id] [--debug] [--x11|--native-wayland]
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--debug") == 0)
        {
            config.renderBackend.enableValidation = true;
        }
        else if (ArgEquals(argv[i], "--x11"))
        {
            forceX11 = true;
        }
        else if (ArgEquals(argv[i], "--native-wayland") || ArgEquals(argv[i], "--wayland"))
        {
            nativeWayland = true;
        }
        else if (ArgEquals(argv[i], "--vulkan"))
        {
            config.renderBackend.preferredAPI = SagaEngine::Render::Backend::GraphicsBackendAPI::kNativePortable;
        }
        else if (ArgEquals(argv[i], "--opengl"))
        {
            config.renderBackend.preferredAPI = SagaEngine::Render::Backend::GraphicsBackendAPI::kCompatibility;
        }
        else if (ArgEquals(argv[i], "--no-mem-report"))
        {
            config.dumpMemoryLeaksOnExit = false;
        }
        else if (ArgEquals(argv[i], "--mem-report"))
        {
            config.dumpMemoryLeaksOnExit = true;
        }
        else if (ArgEquals(argv[i], "--no-scenario"))
        {
            config.initialScenarioId.clear();
        }
        else if (argv[i][0] != '-')
        {
            config.initialScenarioId = argv[i];
        }
    }

    ConfigureLinuxVideoDriver(forceX11, nativeWayland);
    ProbeVulkanLoader();

    const bool dumpMemoryLeaksOnExit = config.dumpMemoryLeaksOnExit;

    {
        SagaSandbox::SandboxHost host(std::move(config));
        host.Run();
    }

    if (dumpMemoryLeaksOnExit)
        SagaEngine::Core::MemoryTracker::Instance().DumpLeaks();
    SagaEngine::Core::MemoryTracker::Shutdown();

    return 0;
}
