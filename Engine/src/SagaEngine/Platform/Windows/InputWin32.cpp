#include "SagaEngine/Platform/Windows/InputWin32.h"
#include "SagaEngine/Platform/IWindow.h"
#include <Windows.h>
#include <stdexcept>
#include <vector>
#include <chrono>

namespace SagaEngine::Platform {

InputWin32::InputWin32() {
    for (int i=0;i<512;i++) keyState[i] = false;
}

InputWin32::~InputWin32() {}

static uint64_t NowMs64() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void InputWin32::AttachWindow(const NativeWindowHandle& handle) {
    if (!handle.ptr) return;
    HWND hwnd = static_cast<HWND>(handle.ptr);
    RAWINPUTDEVICE rid{};
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x06;
    rid.dwFlags = 0;
    rid.hwndTarget = hwnd;
    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
    }
}

void InputWin32::Update() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_CHAR) {
            char ch = static_cast<char>(msg.wParam & 0xFF);
            std::lock_guard<std::mutex> lk(queueMutex);
            pendingText = std::string(1, ch);
        }
        else if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN ||
                 msg.message == WM_KEYUP   || msg.message == WM_SYSKEYUP) {
            bool pressed = (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN);
            unsigned int vk = static_cast<unsigned int>(msg.wParam);
            KeyCode kc = TranslateWinVK(vk);
            uint64_t ts = NowMs64();
            KeyEvent e{ kc, pressed, ts };
            PushKeyEvent(e);
            auto idx = static_cast<uint32_t>(kc);
            if (idx < 512) keyState[idx] = pressed;
        }
        else if (msg.message == WM_INPUT) {
            UINT dwSize = 0;
            GetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
            if (dwSize > 0) {
                std::vector<BYTE> lpb(dwSize);
                if (GetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, lpb.data(), &dwSize, sizeof(RAWINPUTHEADER)) == dwSize) {
                    RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(lpb.data());
                    if (raw->header.dwType == RIM_TYPEKEYBOARD) {
                        RAWKEYBOARD& rk = raw->data.keyboard;
                        unsigned int vk = rk.VKey;
                        bool pressed = !(rk.Flags & RI_KEY_BREAK);
                        KeyCode kc = TranslateWinVK(vk);
                        uint64_t ts = NowMs64();
                        KeyEvent e{ kc, pressed, ts };
                        PushKeyEvent(e);
                        auto idx = static_cast<uint32_t>(kc);
                        if (idx < 512) keyState[idx] = pressed;
                    }
                }
            }
        }
        else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

std::optional<KeyEvent> InputWin32::PollEvent() {
    std::lock_guard<std::mutex> lk(queueMutex);
    if (eventQueue.empty()) return std::nullopt;
    KeyEvent e = eventQueue.front();
    eventQueue.pop();
    return e;
}

bool InputWin32::IsKeyDown(KeyCode key) const {
    uint32_t idx = static_cast<uint32_t>(key);
    if (idx >= 512) return false;
    return keyState[idx];
}

std::optional<std::string> InputWin32::PollTextInput() {
    std::lock_guard<std::mutex> lk(queueMutex);
    if (!pendingText.has_value()) return std::nullopt;
    auto t = pendingText;
    pendingText.reset();
    return t;
}

void InputWin32::PushKeyEvent(const KeyEvent& e) {
    std::lock_guard<std::mutex> lk(queueMutex);
    eventQueue.push(e);
}

KeyCode InputWin32::TranslateWinVK(unsigned int vk) const {
    if (vk >= 'A' && vk <= 'Z') {
        return static_cast<KeyCode>( static_cast<uint32_t>(KeyCode::A) + (vk - 'A') );
    }
    if (vk >= '0' && vk <= '9') {
        return static_cast<KeyCode>( static_cast<uint32_t>(KeyCode::Num0) + (vk - '0') );
    }
    switch (vk) {
    case VK_ESCAPE: return KeyCode::Escape;
    case VK_SPACE: return KeyCode::Space;
    case VK_RETURN: return KeyCode::Enter;
    case VK_BACK: return KeyCode::Backspace;
    case VK_TAB: return KeyCode::Tab;
    case VK_LEFT: return KeyCode::Left;
    case VK_RIGHT: return KeyCode::Right;
    case VK_UP: return KeyCode::Up;
    case VK_DOWN: return KeyCode::Down;
    case VK_LSHIFT: return KeyCode::LShift;
    case VK_RSHIFT: return KeyCode::RShift;
    case VK_LCONTROL: return KeyCode::LCtrl;
    case VK_RCONTROL: return KeyCode::RCtrl;
    default: return KeyCode::Unknown;
    }
}

} // namespace SagaEngine::Platform