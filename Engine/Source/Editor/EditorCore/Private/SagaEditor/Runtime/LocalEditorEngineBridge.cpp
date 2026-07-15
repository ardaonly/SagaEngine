/// @file LocalEditorEngineBridge.cpp
/// @brief In-process editor bridge implementation.

#include "SagaEditor/Runtime/LocalEditorEngineBridge.h"

namespace SagaEditor
{

namespace
{

[[nodiscard]] const char* BuildVersion() noexcept
{
#ifdef SAGA_VERSION
    return SAGA_VERSION;
#else
    return "unknown";
#endif
}

[[nodiscard]] const char* BuildCommit() noexcept
{
#ifdef SAGA_COMMIT
    return SAGA_COMMIT;
#else
    return "unknown";
#endif
}

} // namespace

LocalEditorEngineBridge::LocalEditorEngineBridge()
{
    m_snapshot.displayName = "Local Engine Bridge";
    m_snapshot.runtimeRole = "Editor Evaluation Runtime";
    m_snapshot.engineVersion = BuildVersion();
    m_snapshot.gitCommit = BuildCommit();
    m_snapshot.message = "Not initialised";
}

LocalEditorEngineBridge::~LocalEditorEngineBridge() = default;

bool LocalEditorEngineBridge::Init()
{
    m_snapshot.state = EditorEngineBridgeState::Ready;
    m_snapshot.message = "Engine boundary ready";
    return true;
}

void LocalEditorEngineBridge::Shutdown()
{
    m_snapshot.state = EditorEngineBridgeState::Offline;
    m_snapshot.message = "Engine bridge shut down";
}

EditorEngineBridgeSnapshot LocalEditorEngineBridge::Snapshot() const
{
    return m_snapshot;
}

std::string LocalEditorEngineBridge::StableIdentity() const
{
    return "saga.editor.engine_bridge.local";
}

} // namespace SagaEditor
