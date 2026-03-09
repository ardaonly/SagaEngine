#pragma once

#include "SagaEngine/Platform/IInputDevice.h"
#include "SagaEngine/Platform/IWindow.h"
#include <queue>
#include <mutex>
#include <optional>
#include <string>

namespace SagaEngine::Platform
{

class InputWin32 final : public IInputDevice
{
public:
    InputWin32();
    ~InputWin32() override;
    void Update() override;
    std::optional<KeyEvent> PollEvent() override;
    bool IsKeyDown(KeyCode key) const override;
    std::optional<std::string> PollTextInput() override;
    void AttachWindow(const NativeWindowHandle& handle);

private:
    mutable std::mutex queueMutex;
    std::queue<KeyEvent> eventQueue;
    bool keyState[512];
    std::optional<std::string> pendingText;
    void PushKeyEvent(const KeyEvent& e);
    KeyCode TranslateWinVK(unsigned int vk) const;
};

} // namespace SagaEngine::Platform