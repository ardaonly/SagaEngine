#pragma once
#include <SagaEngine/NativePlatform/IWindow.h>
#include <functional>
#include <string>
#include <memory>

namespace SagaEngine::NativePlatform
{

class WindowWin32 final : public IWindow
{
public:
    WindowWin32();
    ~WindowWin32() override;

    void Create(int width, int height, const char* title) override;
    void Show() override;
    void PollMessages() override;
    NativeWindowHandle GetNativeHandle() const override;

    void SetOnResize(std::function<void(int,int)> cb) override;
    void SetOnClose(std::function<void()> cb) override;
    void SetOnFocus(std::function<void(bool)> cb) override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;
};
}
