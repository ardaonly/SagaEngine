#include "SagaEngine/Platform/Windows/WindowWin32.h"
#include "SagaEngine/Core/Log/Log.h"
#include <stdexcept>
#include <cstdio>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace SagaEngine::Platform {

namespace {
    const char* WINDOW_CLASS_NAME = "SagaEngine_WindowClass_v1";
    std::mutex g_ClassMutex;
    int g_ClassRefCount = 0;
}

struct WindowWin32::Impl {
    std::function<void(int, int)> onResize;
    std::function<void()> onClose;
    std::function<void(bool)> onFocus;
    std::mutex cbMutex;

    static WindowWin32* GetWindowFromHwnd(HWND hwnd) {
        if (!hwnd) return nullptr;
        return reinterpret_cast<WindowWin32*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        #if defined(_DEBUG)
        char debugBuf[128];
        if (msg == WM_CREATE || msg == WM_NCCREATE || msg == WM_SHOWWINDOW) {
            snprintf(debugBuf, sizeof(debugBuf), "[WndProc] MSG: %u, HWND: %p\n", msg, hwnd);
            OutputDebugStringA(debugBuf);
        }
        #endif

        if (msg == WM_NCCREATE) {
            OutputDebugStringA("[WndProc] WM_NCCREATE received\n");
            CREATESTRUCTA* cs = reinterpret_cast<CREATESTRUCTA*>(lParam);
            WindowWin32* self = reinterpret_cast<WindowWin32*>(cs->lpCreateParams);
            if (self) {
                SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
                OutputDebugStringA("[WndProc] GWLP_USERDATA set\n");
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        WindowWin32* self = GetWindowFromHwnd(hwnd);
        if (!self) {
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        switch (msg) {
            case WM_SIZE: {
                int width = LOWORD(lParam);
                int height = HIWORD(lParam);
                {
                    std::lock_guard<std::mutex> lk(self->m_Pimpl->cbMutex);
                    if (self->m_Pimpl->onResize) {
                        self->m_Pimpl->onResize(width, height);
                    }
                }
                std::lock_guard<std::mutex> sk(self->m_StateMutex);
                self->m_Width = width;
                self->m_Height = height;
                return 0;
            }
            case WM_SETFOCUS: {
                std::lock_guard<std::mutex> lk(self->m_Pimpl->cbMutex);
                if (self->m_Pimpl->onFocus) {
                    self->m_Pimpl->onFocus(true);
                }
                return 0;
            }
            case WM_KILLFOCUS: {
                std::lock_guard<std::mutex> lk(self->m_Pimpl->cbMutex);
                if (self->m_Pimpl->onFocus) {
                    self->m_Pimpl->onFocus(false);
                }
                return 0;
            }
            case WM_CLOSE: {
                OutputDebugStringA("[WndProc] WM_CLOSE received\n");
                std::lock_guard<std::mutex> lk(self->m_Pimpl->cbMutex);
                if (self->m_Pimpl->onClose) {
                    self->m_Pimpl->onClose();
                } else {
                    DestroyWindow(hwnd);
                }
                return 0;
            }
            case WM_DESTROY: {
                OutputDebugStringA("[WndProc] WM_DESTROY received\n");
                PostQuitMessage(0);
                return 0;
            }
            default:
                return DefWindowProc(hwnd, msg, wParam, lParam);
        }
    }
};

WindowWin32::WindowWin32()
: m_Hwnd(nullptr)
, m_HInstance(GetModuleHandleA(nullptr))
, m_Width(0)
, m_Height(0)
, m_Initialized(false)
, m_Pimpl(std::make_unique<Impl>()) {
    std::printf("[WindowWin32] Constructor: HINSTANCE=%p\n", m_HInstance);
    std::fflush(stdout);
    OutputDebugStringA("[WindowWin32] Constructor START\n");
    OutputDebugStringA("[WindowWin32] Constructor END\n");
}

WindowWin32::~WindowWin32() {
    OutputDebugStringA("[WindowWin32] Destructor START\n");
    std::printf("[WindowWin32] Destructor: HWND=%p\n", m_Hwnd);
    std::fflush(stdout);
    
    if (m_Hwnd) {
        DestroyWindow(m_Hwnd);
        m_Hwnd = nullptr;
        OutputDebugStringA("[WindowWin32] DestroyWindow called\n");
    }

    {
        std::lock_guard<std::mutex> lk(g_ClassMutex);
        if (--g_ClassRefCount == 0 && m_HInstance) {
            UnregisterWindowClass(m_HInstance);
        }
    }
    
    OutputDebugStringA("[WindowWin32] Destructor END\n");
}

bool WindowWin32::RegisterWindowClass(HINSTANCE hInstance) {
    OutputDebugStringA("[WindowWin32] RegisterWindowClass START\n");
    std::lock_guard<std::mutex> lk(g_ClassMutex);

    if (g_ClassRefCount > 0) {
        g_ClassRefCount++;
        std::printf("[WindowWin32] Class already registered (ref=%d)\n", g_ClassRefCount);
        std::fflush(stdout);
        OutputDebugStringA("[WindowWin32] Class already registered\n");
        return true;
    }

    std::printf("[WindowWin32] Registering window class: %s\n", WINDOW_CLASS_NAME);
    std::fflush(stdout);
    OutputDebugStringA("[WindowWin32] Registering window class\n");

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowWin32::Impl::WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    ATOM atom = RegisterClassExA(&wc);
    if (!atom) {
        DWORD error = GetLastError();
        std::printf("[WindowWin32] RegisterClassEx FAILED: error=%lu\n", error);
        std::fflush(stdout);
        
        char errBuf[256];
        snprintf(errBuf, sizeof(errBuf), "[WindowWin32] RegisterClassEx FAILED: %lu\n", error);
        OutputDebugStringA(errBuf);
        
        return false;
    }

    g_ClassRefCount++;
    std::printf("[WindowWin32] Class registered: atom=%d, ref=%d\n", atom, g_ClassRefCount);
    std::fflush(stdout);
    
    char regBuf[128];
    snprintf(regBuf, sizeof(regBuf), "[WindowWin32] Class registered: atom=%d\n", atom);
    OutputDebugStringA(regBuf);
    
    OutputDebugStringA("[WindowWin32] RegisterWindowClass END\n");
    return true;
}

void WindowWin32::UnregisterWindowClass(HINSTANCE hInstance) {
    OutputDebugStringA("[WindowWin32] UnregisterWindowClass\n");
    UnregisterClassA(WINDOW_CLASS_NAME, hInstance);
    std::printf("[WindowWin32] Class unregistered\n");
    std::fflush(stdout);
}

void WindowWin32::Create(int width, int height, const char* title) {
    OutputDebugStringA("[WindowWin32] Create START\n");
    std::printf("[WindowWin32] Create STARTED: %dx%d, title=%s\n", width, height, title);
    std::fflush(stdout);

    if (m_Initialized.load()) {
        std::printf("[WindowWin32] WARNING: Already initialized\n");
        std::fflush(stdout);
        OutputDebugStringA("[WindowWin32] Already initialized\n");
        return;
    }

    if (!m_HInstance) {
        m_HInstance = GetModuleHandleA(nullptr);
        std::printf("[WindowWin32] HInstance acquired: %p\n", m_HInstance);
        std::fflush(stdout);
        OutputDebugStringA("[WindowWin32] HInstance acquired\n");
    }

    OutputDebugStringA("[WindowWin32] Calling RegisterWindowClass\n");
    if (!RegisterWindowClass(m_HInstance)) {
        OutputDebugStringA("[WindowWin32] RegisterWindowClass FAILED\n");
        throw std::runtime_error("Failed to register window class");
    }
    OutputDebugStringA("[WindowWin32] RegisterWindowClass SUCCESS\n");

    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rect = {0, 0, width, height};
    
    std::printf("[WindowWin32] Adjusting window rect...\n");
    std::fflush(stdout);
    OutputDebugStringA("[WindowWin32] AdjustWindowRect\n");
    
    BOOL adjustResult = AdjustWindowRect(&rect, style, FALSE);
    if (!adjustResult) {
        DWORD error = GetLastError();
        std::printf("[WindowWin32] AdjustWindowRect FAILED: error=%lu\n", error);
        std::fflush(stdout);
        char errBuf[256];
        snprintf(errBuf, sizeof(errBuf), "[WindowWin32] AdjustWindowRect FAILED: %lu\n", error);
        OutputDebugStringA(errBuf);
    }

    int winWidth = rect.right - rect.left;
    int winHeight = rect.bottom - rect.top;

    std::printf("[WindowWin32] Creating window: %dx%d (client: %dx%d)\n", 
                winWidth, winHeight, width, height);
    std::printf("[WindowWin32] HInstance=%p, This=%p\n", m_HInstance, this);
    std::fflush(stdout);

    char createBuf[256];
    snprintf(createBuf, sizeof(createBuf), "[WindowWin32] CreateWindowEx: %dx%d\n", winWidth, winHeight);
    OutputDebugStringA(createBuf);

    m_Hwnd = CreateWindowExA(
        0,
        WINDOW_CLASS_NAME,
        title,
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        winWidth,
        winHeight,
        nullptr,
        nullptr,
        m_HInstance,
        this
    );

    if (!m_Hwnd) {
        DWORD error = GetLastError();
        std::printf("[WindowWin32] CreateWindowEx FAILED: error=%lu\n", error);
        std::fflush(stdout);
        
        char errBuf[512];
        snprintf(errBuf, sizeof(errBuf), "[WindowWin32] CreateWindowEx FAILED: %lu\n", error);
        OutputDebugStringA(errBuf);
        
        std::printf("[WindowWin32] Error meanings:\n");
        std::printf("  1400 = Invalid window handle\n");
        std::printf("  1407 = Cannot find window class\n");
        std::printf("  1410 = Class already registered\n");
        std::fflush(stdout);
        
        throw std::runtime_error("Window creation failed");
    }

    std::printf("[WindowWin32] CreateWindowEx SUCCESS: HWND=%p\n", m_Hwnd);
    std::fflush(stdout);
    
    char hwndBuf[128];
    snprintf(hwndBuf, sizeof(hwndBuf), "[WindowWin32] CreateWindowEx SUCCESS: HWND=%p\n", m_Hwnd);
    OutputDebugStringA(hwndBuf);

    LONG_PTR userData = GetWindowLongPtr(m_Hwnd, GWLP_USERDATA);
    std::printf("[WindowWin32] GWLP_USERDATA check: %p (expected: %p)\n", 
                reinterpret_cast<void*>(userData), this);
    std::fflush(stdout);
    
    char userDataBuf[256];
    snprintf(userDataBuf, sizeof(userDataBuf), "[WindowWin32] GWLP_USERDATA: %p (expected: %p)\n", 
             reinterpret_cast<void*>(userData), this);
    OutputDebugStringA(userDataBuf);

    BOOL isVisible = IsWindowVisible(m_Hwnd);
    std::printf("[WindowWin32] IsWindowVisible: %d\n", isVisible);
    std::fflush(stdout);

    std::lock_guard<std::mutex> lk(m_StateMutex);
    m_Width = width;
    m_Height = height;
    m_Title = title;
    m_Initialized.store(true);

    std::printf("[WindowWin32] Create COMPLETE: HWND=%p, Size=%dx%d\n", 
                m_Hwnd, m_Width, m_Height);
    std::fflush(stdout);
    OutputDebugStringA("[WindowWin32] Create COMPLETE\n");
}

void WindowWin32::Show() {
    OutputDebugStringA("[WindowWin32] Show START\n");
    std::printf("[WindowWin32] Show STARTED: HWND=%p, Valid=%d\n", 
                m_Hwnd, m_Initialized.load());
    std::fflush(stdout);

    if (!m_Hwnd) {
        std::printf("[WindowWin32] Show FAILED: HWND is null\n");
        std::fflush(stdout);
        OutputDebugStringA("[WindowWin32] Show FAILED: HWND is null\n");
        return;
    }

    OutputDebugStringA("[WindowWin32] Calling ShowWindow\n");
    int showResult = ShowWindow(m_Hwnd, SW_SHOWDEFAULT);
    std::printf("[WindowWin32] ShowWindow result: %d\n", showResult);
    std::fflush(stdout);
    
    char showBuf[128];
    snprintf(showBuf, sizeof(showBuf), "[WindowWin32] ShowWindow result: %d\n", showResult);
    OutputDebugStringA(showBuf);

    OutputDebugStringA("[WindowWin32] Calling UpdateWindow\n");
    BOOL updateResult = UpdateWindow(m_Hwnd);
    std::printf("[WindowWin32] UpdateWindow result: %d\n", updateResult);
    std::fflush(stdout);

    BOOL isVisible = IsWindowVisible(m_Hwnd);
    std::printf("[WindowWin32] IsWindowVisible after Show: %d\n", isVisible);
    std::fflush(stdout);

    std::printf("[WindowWin32] Show COMPLETE\n");
    std::fflush(stdout);
    OutputDebugStringA("[WindowWin32] Show COMPLETE\n");
}

void WindowWin32::PollMessages() {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

NativeWindowHandle WindowWin32::GetNativeHandle() const {
    NativeWindowHandle handle;
    handle.ptr = static_cast<void*>(const_cast<HWND>(m_Hwnd));
    return handle;
}

void WindowWin32::SetOnResize(std::function<void(int, int)> cb) {
    std::lock_guard<std::mutex> lk(m_Pimpl->cbMutex);
    m_Pimpl->onResize = std::move(cb);
}

void WindowWin32::SetOnClose(std::function<void()> cb) {
    std::lock_guard<std::mutex> lk(m_Pimpl->cbMutex);
    m_Pimpl->onClose = std::move(cb);
}

void WindowWin32::SetOnFocus(std::function<void(bool)> cb) {
    std::lock_guard<std::mutex> lk(m_Pimpl->cbMutex);
    m_Pimpl->onFocus = std::move(cb);
}

bool WindowWin32::IsValid() const {
    return m_Initialized.load() && m_Hwnd != nullptr;
}

int WindowWin32::GetWidth() const {
    std::lock_guard<std::mutex> lk(m_StateMutex);
    return m_Width;
}

int WindowWin32::GetHeight() const {
    std::lock_guard<std::mutex> lk(m_StateMutex);
    return m_Height;
}

const char* WindowWin32::GetTitle() const {
    std::lock_guard<std::mutex> lk(m_StateMutex);
    return m_Title.c_str();
}

void* WindowWin32::GetPlatformHandle() const {
    return static_cast<void*>(const_cast<HWND>(m_Hwnd));
}

void* WindowWin32::GetPlatformInstance() const {
    return static_cast<void*>(m_HInstance);
}

}