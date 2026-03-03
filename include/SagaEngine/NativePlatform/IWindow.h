// include/SagaEngine/NativePlatform/IWindow.h
#pragma once
#include <functional>
#include <memory>
#include <string>

namespace SagaEngine::NativePlatform
{

struct NativeWindowHandle { void* ptr = nullptr; };

class IWindow
{
public:
    virtual ~IWindow() = default;

    virtual void Create(int width, int height, const char* title) = 0;

    virtual void Show() = 0;

    virtual void PollMessages() = 0;

    virtual NativeWindowHandle GetNativeHandle() const = 0;

    virtual void SetOnResize(std::function<void(int,int)> cb) = 0;
    virtual void SetOnClose(std::function<void()> cb) = 0;
    virtual void SetOnFocus(std::function<void(bool focused)> cb) = 0;
};
}
