/// @file LocalEditorEngineBridge.h
/// @brief In-process editor bridge used by the first production editor slice.

#pragma once

#include "SagaEditor/Runtime/IEditorEngineBridge.h"

namespace SagaEditor
{

class LocalEditorEngineBridge final : public IEditorEngineBridge
{
public:
    LocalEditorEngineBridge();
    ~LocalEditorEngineBridge() override;

    [[nodiscard]] bool Init() override;
    void Shutdown() override;

    [[nodiscard]] EditorEngineBridgeSnapshot Snapshot() const override;
    [[nodiscard]] std::string StableIdentity() const override;

private:
    EditorEngineBridgeSnapshot m_snapshot;
};

} // namespace SagaEditor
