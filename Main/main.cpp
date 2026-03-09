#include "SagaEngine/Core/Application/Application.h"
#include "SagaEngine/Core/Log/Log.h"
#include "SagaEngine/Platform/PlatformFactory.h"
#include "SagaEngine/Platform/IWindow.h"

#include <iostream>
#include <memory>
#include <csignal>
#include <exception>
#include <atomic>
#include <cstdio>

#if defined(_WIN32)
#include <Windows.h>
#endif

using namespace SagaEngine;

namespace {
    std::atomic<Core::Application*> g_AppPtr{nullptr};

    void SignalHandler(int signal) {
        if (signal == SIGINT || signal == SIGTERM) {
            #if defined(_WIN32)
            OutputDebugStringA("[Signal] Shutdown requested\n");
            #endif
            Core::Application* app = g_AppPtr.load(std::memory_order_acquire);
            if (app) {
                app->Close();
            }
        }
    }
}

class SagaGameModule : public Core::Application {
    std::unique_ptr<Platform::IWindow> m_Window;
    uint64_t m_FrameCount = 0;

public:
    SagaGameModule() : Application("SagaEngine Diagnostic") {
        #if defined(_WIN32)
        OutputDebugStringA("[SagaGameModule] Constructor START\n");
        #endif
        
        std::printf("[SagaGameModule] Constructor START\n");
        std::fflush(stdout);
        
        #if defined(_WIN32)
        OutputDebugStringA("[SagaGameModule] Constructor END\n");
        #endif
        
        std::printf("[SagaGameModule] Constructor END\n");
        std::fflush(stdout);
    }

    ~SagaGameModule() {
        #if defined(_WIN32)
        OutputDebugStringA("[SagaGameModule] Destructor START\n");
        #endif
        
        std::printf("[SagaGameModule] Destructor START\n");
        std::fflush(stdout);
        
        #if defined(_WIN32)
        OutputDebugStringA("[SagaGameModule] Destructor END\n");
        #endif
        
        std::printf("[SagaGameModule] Destructor END\n");
        std::fflush(stdout);
    }

    void OnInit() override {
        #if defined(_WIN32)
        OutputDebugStringA("[OnInit] ===== START =====\n");
        #endif
        
        std::printf("[OnInit] ===== START =====\n");
        std::fflush(stdout);

        try {
            std::printf("[OnInit] Step 1: Creating platform objects...\n");
            std::fflush(stdout);
            #if defined(_WIN32)
            OutputDebugStringA("[OnInit] Step 1: Creating platform objects\n");
            #endif

            auto objects = Platform::CreatePlatformObjects();

            std::printf("[OnInit] Step 2: Platform objects created\n");
            std::printf("[OnInit]   Window ptr: %p\n", objects.window.get());
            std::fflush(stdout);
            #if defined(_WIN32)
            OutputDebugStringA("[OnInit] Step 2: Platform objects created\n");
            #endif

            if (!objects.window) {
                std::printf("[OnInit] CRITICAL: Window is NULL\n");
                std::fflush(stdout);
                #if defined(_WIN32)
                OutputDebugStringA("[OnInit] CRITICAL: Window is NULL\n");
                #endif
                m_Running = false;
                return;
            }

            m_Window = std::move(objects.window);

            std::printf("[OnInit] Step 3: Moving window pointer\n");
            std::printf("[OnInit]   m_Window ptr: %p\n", m_Window.get());
            std::fflush(stdout);
            #if defined(_WIN32)
            OutputDebugStringA("[OnInit] Step 3: Moving window pointer\n");
            #endif

            std::printf("[OnInit] Step 4: Calling Create(1280, 720)...\n");
            std::fflush(stdout);
            #if defined(_WIN32)
            OutputDebugStringA("[OnInit] Step 4: Calling Create\n");
            #endif

            m_Window->Create(1280, 720, "SagaEngine MMO Client");

            std::printf("[OnInit] Step 5: Create() returned\n");
            std::fflush(stdout);
            #if defined(_WIN32)
            OutputDebugStringA("[OnInit] Step 5: Create() returned\n");
            #endif

            std::printf("[OnInit] Step 6: Validating window...\n");
            std::printf("[OnInit]   IsValid: %d\n", m_Window->IsValid());
            std::printf("[OnInit]   Handle: %p\n", m_Window->GetPlatformHandle());
            std::fflush(stdout);
            #if defined(_WIN32)
            OutputDebugStringA("[OnInit] Step 6: Validating window\n");
            #endif

            #if defined(_WIN32)
            void* hwnd = m_Window->GetPlatformHandle();
            if (hwnd) {
                std::printf("[OnInit]   IsWindow: %d\n", IsWindow((HWND)hwnd));
                std::printf("[OnInit]   IsWindowVisible: %d\n", IsWindowVisible((HWND)hwnd));
                OutputDebugStringA("[OnInit] Window validation complete\n");
            }
            #endif

            if (!m_Window->IsValid()) {
                std::printf("[OnInit] CRITICAL: Window not valid after Create()\n");
                std::fflush(stdout);
                #if defined(_WIN32)
                OutputDebugStringA("[OnInit] CRITICAL: Window not valid\n");
                #endif
                m_Running = false;
                return;
            }

            std::printf("[OnInit] Step 7: Setting close callback...\n");
            std::fflush(stdout);
            #if defined(_WIN32)
            OutputDebugStringA("[OnInit] Step 7: Setting close callback\n");
            #endif

            m_Window->SetOnClose([this]() {
                std::printf("[Window] Close callback triggered!\n");
                std::fflush(stdout);
                #if defined(_WIN32)
                OutputDebugStringA("[Window] Close callback triggered\n");
                #endif
                Close();
            });

            std::printf("[OnInit] Step 8: Calling Show()...\n");
            std::fflush(stdout);
            #if defined(_WIN32)
            OutputDebugStringA("[OnInit] Step 8: Calling Show\n");
            #endif

            m_Window->Show();

            std::printf("[OnInit] Step 9: Show() returned\n");
            std::fflush(stdout);
            #if defined(_WIN32)
            OutputDebugStringA("[OnInit] Step 9: Show() returned\n");
            #endif

            std::printf("[OnInit] Step 10: Final validation...\n");
            std::printf("[OnInit]   Width: %d\n", m_Window->GetWidth());
            std::printf("[OnInit]   Height: %d\n", m_Window->GetHeight());
            std::printf("[OnInit]   Title: %s\n", m_Window->GetTitle());
            std::fflush(stdout);
            #if defined(_WIN32)
            OutputDebugStringA("[OnInit] Step 10: Final validation\n");
            #endif

            std::printf("[OnInit] ===== COMPLETE =====\n");
            std::fflush(stdout);
            #if defined(_WIN32)
            OutputDebugStringA("[OnInit] ===== COMPLETE =====\n");
            #endif
        }
        catch (const std::exception& e) {
            std::printf("[OnInit] EXCEPTION: %s\n", e.what());
            std::fflush(stdout);
            #if defined(_WIN32)
            char buf[512];
            snprintf(buf, sizeof(buf), "[OnInit] EXCEPTION: %s\n", e.what());
            OutputDebugStringA(buf);
            #endif
            m_Running = false;
        }
        catch (...) {
            std::printf("[OnInit] UNKNOWN EXCEPTION\n");
            std::fflush(stdout);
            #if defined(_WIN32)
            OutputDebugStringA("[OnInit] UNKNOWN EXCEPTION\n");
            #endif
            m_Running = false;
        }
    }

    void OnUpdate(float deltaTime) override {
        m_FrameCount++;

        if (m_FrameCount <= 5 || m_FrameCount % 60 == 0) {
            std::printf("[OnUpdate] Frame %llu | DT: %.4fms | HWND: %p\n", 
                       m_FrameCount, 
                       deltaTime * 1000.0f,
                       m_Window ? m_Window->GetPlatformHandle() : nullptr);
            std::fflush(stdout);
        }

        if (m_Window) {
            m_Window->PollMessages();
        }

        if (m_FrameCount >= 500) {
            std::printf("[OnUpdate] Frame 500 reached - Auto closing\n");
            std::fflush(stdout);
            Close();
        }
    }

    void OnShutdown() override {
        #if defined(_WIN32)
        OutputDebugStringA("[OnShutdown] ===== START =====\n");
        #endif
        
        std::printf("[OnShutdown] ===== START =====\n");
        std::printf("[OnShutdown] Total Frames: %llu\n", m_FrameCount);
        std::fflush(stdout);

        m_Window.reset();

        std::printf("[OnShutdown] ===== COMPLETE =====\n");
        std::fflush(stdout);
        #if defined(_WIN32)
        OutputDebugStringA("[OnShutdown] ===== COMPLETE =====\n");
        #endif
    }
};

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show) {
    (void)hInst; (void)hPrev; (void)cmd; (void)show;

    #if defined(_DEBUG)
    if (GetConsoleWindow() == nullptr) {
        if (AllocConsole()) {
            freopen("CONOUT$", "w", stdout);
            freopen("CONOUT$", "w", stderr);
            freopen("CONIN$", "r", stdin);
            std::printf("[WinMain] Console allocated\n");
            std::fflush(stdout);
            OutputDebugStringA("[WinMain] Console allocated\n");
        }
    }
    #endif

    OutputDebugStringA("\n================================================\n");
    OutputDebugStringA("       SAGAENGINE DIAGNOSTIC BOOT\n");
    OutputDebugStringA("================================================\n");

    std::printf("\n================================================\n");
    std::printf("       SAGAENGINE DIAGNOSTIC BOOT\n");
    std::printf("================================================\n");
    std::printf("Build: %s %s\n", __DATE__, __TIME__);
    std::printf("Platform: Windows (Win32)\n");
    std::printf("HINSTANCE: %p\n", hInst);
    std::printf("================================================\n\n");
    std::fflush(stdout);

    OutputDebugStringA("[WinMain] Step 1: Signal handlers\n");
    std::printf("[WinMain] Step 1: Setting up signal handlers...\n");
    std::fflush(stdout);

    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    OutputDebugStringA("[WinMain] Step 2: Log system\n");
    std::printf("[WinMain] Step 2: Initializing log system...\n");
    std::fflush(stdout);

    try {
        Core::Log::Init("saga_engine.log");
        std::printf("[WinMain] Step 3: Log system initialized\n");
        std::fflush(stdout);
        OutputDebugStringA("[WinMain] Step 3: Log system initialized\n");
    }
    catch (...) {
        std::printf("[WinMain] Log::Init() FAILED\n");
        std::fflush(stdout);
        OutputDebugStringA("[WinMain] Log::Init() FAILED\n");
    }

    OutputDebugStringA("[WinMain] Step 4: Creating application...\n");
    std::printf("[WinMain] Step 4: Creating application...\n");
    std::fflush(stdout);

    try {
        SagaGameModule app;
        OutputDebugStringA("[WinMain] Step 5: Application created\n");
        std::printf("[WinMain] Step 5: Application created\n");
        std::fflush(stdout);

        g_AppPtr.store(&app, std::memory_order_release);
        
        OutputDebugStringA("[WinMain] Step 6: Running application...\n");
        std::printf("[WinMain] Step 6: Running application...\n");
        std::fflush(stdout);

        app.Run();

        g_AppPtr.store(nullptr, std::memory_order_release);
        
        OutputDebugStringA("[WinMain] Step 7: Application finished\n");
        std::printf("[WinMain] Step 7: Application finished\n");
        std::fflush(stdout);
    }
    catch (const std::exception& e) {
        std::printf("[WinMain] FATAL EXCEPTION: %s\n", e.what());
        std::fflush(stdout);
        char buf[512];
        snprintf(buf, sizeof(buf), "[WinMain] FATAL EXCEPTION: %s\n", e.what());
        OutputDebugStringA(buf);
        MessageBoxA(nullptr, e.what(), "SagaEngine Fatal Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    catch (...) {
        std::printf("[WinMain] FATAL: Unknown exception\n");
        std::fflush(stdout);
        OutputDebugStringA("[WinMain] FATAL: Unknown exception\n");
        MessageBoxA(nullptr, "Unknown Exception", "SagaEngine Fatal Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    std::printf("\n[WinMain] Press Enter to exit...\n");
    std::fflush(stdout);
    OutputDebugStringA("[WinMain] Waiting for Enter...\n");
    std::getchar();

    #if defined(_DEBUG)
    FreeConsole();
    #endif

    return 0;
}
#else
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    std::printf("[main] SagaEngine Diagnostic Boot\n");
    std::fflush(stdout);

    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    try {
        Core::Log::Init("saga_engine.log");
        SagaGameModule app;
        g_AppPtr.store(&app, std::memory_order_release);
        app.Run();
        g_AppPtr.store(nullptr, std::memory_order_release);
    }
    catch (const std::exception& e) {
        std::printf("[main] FATAL: %s\n", e.what());
        std::fflush(stdout);
        return 1;
    }
    return 0;
}
#endif