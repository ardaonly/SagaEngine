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
#include <filesystem>
#include <new>
#include <optional>
#include <string>
#include <vector>

#if defined(__linux__)
#   include <dlfcn.h>
#   include <unistd.h>
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

bool ProbeVulkanLoader()
{
#if defined(__linux__)
    static void* loader = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_GLOBAL);
    if (!loader)
    {
        const char* err = dlerror();
        LOG_WARN("SandboxMain",
                 "libvulkan.so.1 could not be loaded before Diligent init: %s. On NixOS, re-enter shell.nix so LD_LIBRARY_PATH includes vulkan-loader/lib.",
                 err ? err : "unknown error");
        return false;
    }
    else
    {
        LOG_INFO("SandboxMain", "Vulkan loader preloaded: libvulkan.so.1");
        return true;
    }
#else
    return true;
#endif
}

#if defined(__linux__)
std::string ShellQuote(const std::string& value)
{
    std::string quoted;
    quoted.reserve(value.size() + 2);
    quoted.push_back('\'');
    for (char ch : value)
    {
        if (ch == '\'')
            quoted += "'\\''";
        else
            quoted.push_back(ch);
    }
    quoted.push_back('\'');
    return quoted;
}

std::optional<std::filesystem::path> FindRepoRoot()
{
    std::filesystem::path dir;
    try
    {
        dir = std::filesystem::current_path();
    }
    catch (...)
    {
        return std::nullopt;
    }

    for (int depth = 0; depth < 12 && !dir.empty(); ++depth)
    {
        if (std::filesystem::exists(dir / "shell.nix") &&
            std::filesystem::exists(dir / "VERSION"))
        {
            return dir;
        }

        const auto parent = dir.parent_path();
        if (parent == dir)
            break;
        dir = parent;
    }

    return std::nullopt;
}

std::filesystem::path CurrentExecutablePath(const char* argv0)
{
    try
    {
        const auto procPath = std::filesystem::read_symlink("/proc/self/exe");
        if (!procPath.empty())
            return procPath;
    }
    catch (...)
    {
    }

    return argv0 && argv0[0] != '\0'
        ? std::filesystem::path(argv0)
        : std::filesystem::path("SagaSandbox");
}

bool TryReexecInsideNixShell(int argc, char* argv[], bool vulkanLoaderAvailable)
{
    if (vulkanLoaderAvailable || HasEnv("SAGA_NIX_REEXEC"))
        return false;

    const auto repoRoot = FindRepoRoot();
    if (!repoRoot)
        return false;

    std::filesystem::path currentDir;
    try
    {
        currentDir = std::filesystem::current_path();
    }
    catch (...)
    {
        return false;
    }

    const auto executable = CurrentExecutablePath(argc > 0 ? argv[0] : nullptr);

    std::string command = "cd ";
    command += ShellQuote(currentDir.string());
    command += " && export SAGA_NIX_REEXEC=1 && exec ";
    command += ShellQuote(executable.string());

    for (int i = 1; i < argc; ++i)
    {
        command.push_back(' ');
        command += ShellQuote(argv[i] ? std::string(argv[i]) : std::string{});
    }

    const std::string shellPath = (*repoRoot / "shell.nix").string();
    const std::string repoRootPath = repoRoot->string();
    LOG_INFO("SandboxMain",
             "Re-entering nix-shell so Vulkan loader and GPU runtime libraries are visible.");

    if (chdir(repoRootPath.c_str()) != 0)
    {
        LOG_WARN("SandboxMain",
                 "Failed to switch to repo root before nix-shell re-entry; run from repo root with: nix-shell --run \"build/RelWithDebInfo-0.0.9/bin/SagaSandbox\"");
        return false;
    }

    std::vector<char*> execArgs;
    execArgs.push_back(const_cast<char*>("nix-shell"));
    execArgs.push_back(const_cast<char*>(shellPath.c_str()));
    execArgs.push_back(const_cast<char*>("--run"));
    execArgs.push_back(const_cast<char*>(command.c_str()));
    execArgs.push_back(nullptr);

    execvp("nix-shell", execArgs.data());
    LOG_WARN("SandboxMain",
             "Failed to re-enter nix-shell automatically; run from repo root with: nix-shell --run \"build/RelWithDebInfo-0.0.9/bin/SagaSandbox\"");
    return false;
}
#else
bool TryReexecInsideNixShell(int, char**, bool)
{
    return false;
}
#endif

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
    config.renderBackend.preferredAPI = SagaEngine::Render::Backend::GraphicsBackendAPI::kNativePortable;

    bool forceX11       = false;
    bool nativeWayland  = false;

    // Parse command line: [scenario_id] [--debug] [--vulkan|--opengl|--auto-api] [--x11|--native-wayland]
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
        else if (ArgEquals(argv[i], "--auto-api"))
        {
            config.renderBackend.preferredAPI = SagaEngine::Render::Backend::GraphicsBackendAPI::kAuto;
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
    const bool vulkanLoaderAvailable = ProbeVulkanLoader();
    TryReexecInsideNixShell(argc, argv, vulkanLoaderAvailable);

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
