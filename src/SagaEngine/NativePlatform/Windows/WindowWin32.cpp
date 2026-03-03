#include <SagaEngine/NativePlatform/Windows/WindowWin32.h>
#include <Windows.h>
#include <stdexcept>
#include <mutex>

namespace SagaEngine::NativePlatform
{

namespace {
const char* kWindowClassName = "SagaEngine_WindowClass_v1";

struct GlobalRegistrar {
    GlobalRegistrar() {
        WNDCLASSEX wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = kWindowClassName;
        RegisterClassEx(&wc);
    }
    ~GlobalRegistrar() {
        UnregisterClass(kWindowClassName, GetModuleHandle(nullptr));
    }
};
static GlobalRegistrar g_registrar;
}

struct WindowWin32::Impl
{
    HWND hwnd = nullptr;
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    std::function<void(int,int)> onResize;
    std::function<void()> onClose;
    std::function<void(bool)> onFocus;
    std::mutex cbMutex;

    static void Associate(HWND hwnd, Impl* self) {
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    static Impl* FromHWND(HWND hwnd) {
        return reinterpret_cast<Impl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        Impl* self = FromHWND(hwnd);
        switch(uMsg) {
            case WM_SIZE:
                if (self) {
                    int w = LOWORD(lParam);
                    int h = HIWORD(lParam);
                    std::lock_guard<std::mutex> lk(self->cbMutex);
                    if(self->onResize) self->onResize(w,h);
                }
                return 0;
            case WM_SETFOCUS:
                if (self) { std::lock_guard<std::mutex> lk(self->cbMutex); if(self->onFocus) self->onFocus(true); }
                return 0;
            case WM_KILLFOCUS:
                if (self) { std::lock_guard<std::mutex> lk(self->cbMutex); if(self->onFocus) self->onFocus(false); }
                return 0;
            case WM_CLOSE:
                if (self) { std::lock_guard<std::mutex> lk(self->cbMutex); if(self->onClose) self->onClose(); }
                PostQuitMessage(0);
                return 0;
            default:
                return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
    }
};

WindowWin32::WindowWin32() : pimpl(std::make_unique<Impl>()) {}
WindowWin32::~WindowWin32() {
    if (pimpl->hwnd) {
        DestroyWindow(pimpl->hwnd);
        pimpl->hwnd = nullptr;
    }
}

void WindowWin32::Create(int width, int height, const char* title)
{
    HWND hwnd = CreateWindowExA(
        0,
        kWindowClassName,
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        nullptr,
        nullptr,
        pimpl->hInstance,
        nullptr
    );
    if (!hwnd) {
        throw std::runtime_error("CreateWindowEx failed");
    }

    LONG_PTR prevProc = SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(Impl::WndProc));
    (void)prevProc;
    Impl::Associate(hwnd, pimpl.get());
    pimpl->hwnd = hwnd;
}

void WindowWin32::Show()
{
    if (!pimpl->hwnd) return;
    ShowWindow(pimpl->hwnd, SW_SHOWDEFAULT);
    UpdateWindow(pimpl->hwnd);
}

void WindowWin32::PollMessages()
{
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

NativeWindowHandle WindowWin32::GetNativeHandle() const
{
    NativeWindowHandle h;
    h.ptr = static_cast<void*>(pimpl->hwnd);
    return h;
}

void WindowWin32::SetOnResize(std::function<void(int,int)> cb) { std::lock_guard<std::mutex> lk(pimpl->cbMutex); pimpl->onResize = std::move(cb); }
void WindowWin32::SetOnClose(std::function<void()> cb) { std::lock_guard<std::mutex> lk(pimpl->cbMutex); pimpl->onClose = std::move(cb); }
void WindowWin32::SetOnFocus(std::function<void(bool)> cb) { std::lock_guard<std::mutex> lk(pimpl->cbMutex); pimpl->onFocus = std::move(cb); }

}
