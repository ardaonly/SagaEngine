#pragma once
#include <functional>
#include <string>

namespace SagaEngine::Platform {

struct NativeWindowHandle {
    void* ptr = nullptr;
};

class IWindow {
public:
    virtual ~IWindow() = default;
    virtual void Create(int width, int height, const char* title) = 0;
    virtual void Show() = 0;
    virtual void PollMessages() = 0;
    virtual NativeWindowHandle GetNativeHandle() const = 0;
    virtual void SetOnResize(std::function<void(int, int)> cb) = 0;
    virtual void SetOnClose(std::function<void()> cb) = 0;
    virtual void SetOnFocus(std::function<void(bool)> cb) = 0;
    
    virtual bool IsValid() const = 0;
    virtual int GetWidth() const = 0;
    virtual int GetHeight() const = 0;
    virtual const char* GetTitle() const = 0;
    
    virtual void* GetPlatformHandle() const = 0;
    virtual void* GetPlatformInstance() const = 0;
};

}