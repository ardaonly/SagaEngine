/// @file KeyboardDevice.cpp

#include "SagaEngine/Input/Devices/KeyboardDevice.h"
#include "SagaEngine/Input/Frames/RawInputFrame.h"

namespace SagaEngine::Input
{

KeyboardDevice::KeyboardDevice(DeviceId id)
    : m_id(id)
{
    m_state.deviceType = DeviceType::Keyboard;
    m_state.deviceId   = id;
}

DeviceType       KeyboardDevice::GetType() const noexcept { return DeviceType::Keyboard; }
DeviceId         KeyboardDevice::GetId()   const noexcept { return m_id; }
std::string_view KeyboardDevice::GetName() const noexcept { return "Keyboard"; }

void KeyboardDevice::BeginFrame() noexcept
{
    m_state.FlipFrame();
    m_textEvents.clear();
    // m_activeModifiers is rebuilt from key events each frame — reset here.
    m_activeModifiers = ModifierFlags::None;
}

void KeyboardDevice::ConsumeFrame(const RawInputFrame& frame)
{
    m_state.lastUpdateUs = frame.frameTimestampUs;

    // Apply all key events in arrival order.
    for (const auto& ev : frame.keys)
    {
        m_state.keyboard.SetDown(ev.code, ev.pressed);

        // Rebuild modifier flags from dedicated modifier keys.
        // We track the physical key state rather than using the event's
        // modifiers field so we don't depend on backend accuracy.
        const auto UpdateMod = [&](KeyCode key, ModifierFlags flag)
        {
            if (ev.code == key)
            {
                if (ev.pressed)
                    m_activeModifiers = m_activeModifiers | flag;
                else
                    m_activeModifiers = static_cast<ModifierFlags>(
                        static_cast<uint8_t>(m_activeModifiers)
                        & ~static_cast<uint8_t>(flag)
                    );
            }
        };

        UpdateMod(KeyCode::LShift, ModifierFlags::Shift);
        UpdateMod(KeyCode::RShift, ModifierFlags::Shift);
        UpdateMod(KeyCode::LCtrl,  ModifierFlags::Ctrl);
        UpdateMod(KeyCode::RCtrl,  ModifierFlags::Ctrl);
        UpdateMod(KeyCode::LAlt,   ModifierFlags::Alt);
        UpdateMod(KeyCode::RAlt,   ModifierFlags::Alt);
        UpdateMod(KeyCode::LMeta,  ModifierFlags::Meta);
        UpdateMod(KeyCode::RMeta,  ModifierFlags::Meta);
    }

    // Collect text events (IME-safe, UI input mode).
    for (const auto& te : frame.text)
        m_textEvents.push_back(te);
}

const InputState& KeyboardDevice::GetState() const noexcept
{
    return m_state;
}

// ─── Factory ─────────────────────────────────────────────────────────────────

std::unique_ptr<KeyboardDevice> MakeKeyboardDevice(DeviceId id)
{
    return std::make_unique<KeyboardDevice>(id);
}

} // namespace SagaEngine::Input