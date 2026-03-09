#pragma once
#include "SagaEngine/Platform/IWindow.h"
#include <Windows.h>
#include <memory>
#include <string>
#include <functional>
#include <mutex>
#include <atomic>

namespace SagaEngine::Platform {

class WindowWin32 final : public IWindow {
public:
    WindowWin32();
    ~WindowWin32() override;

    void Create(int width, int height, const char* title) override;
    void Show() override;
    void PollMessages() override;
    NativeWindowHandle GetNativeHandle() const override;
    void SetOnResize(std::function<void(int, int)> cb) override;
    void SetOnClose(std::function<void()> cb) override;
    void SetOnFocus(std::function<void(bool)> cb) override;

    bool IsValid() const override;
    int GetWidth() const override;
    int GetHeight() const override;
    const char* GetTitle() const override;
    
    void* GetPlatformHandle() const override;
    void* GetPlatformInstance() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Pimpl;
    
    HWND m_Hwnd;
    HINSTANCE m_HInstance;
    int m_Width;
    int m_Height;
    std::string m_Title;
    std::atomic<bool> m_Initialized;
    
    mutable std::mutex m_StateMutex;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static bool RegisterWindowClass(HINSTANCE hInstance);
    static void UnregisterWindowClass(HINSTANCE hInstance);
};

}