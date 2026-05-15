/// @file IEditorEngineBridge.cpp
/// @brief Editor engine bridge helpers.

#include "SagaEditor/Runtime/IEditorEngineBridge.h"

namespace SagaEditor
{

const char* ToString(EditorEngineBridgeState state) noexcept
{
    switch (state)
    {
        case EditorEngineBridgeState::Offline: return "Offline";
        case EditorEngineBridgeState::Starting: return "Starting";
        case EditorEngineBridgeState::Ready: return "Ready";
        case EditorEngineBridgeState::Failed: return "Failed";
    }
    return "Unknown";
}

} // namespace SagaEditor
