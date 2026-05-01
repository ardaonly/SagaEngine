/// @file IPropertyEditorBackend.h
/// @brief Pluggable factory backend that produces concrete property editors.

#pragma once

#include "SagaEditor/InspectorEditing/PropertyEditorFactory.h"

#include <memory>

namespace SagaEditor
{

class IPropertyEditor;

// ─── Property Editor Backend ──────────────────────────────────────────────────

/// One concrete implementation exists per UI backend (Qt today,
/// possibly a future ImGui or web backend). The `PropertyEditorFactory`
/// dispatches every `Create` request through the currently installed
/// backend; if no backend has been installed the factory returns
/// nullptr and the inspector falls back to a read-only label. The
/// indirection keeps `PropertyEditorFactory` framework-free without
/// forcing every caller to know which backend is active.
class IPropertyEditorBackend
{
public:
    virtual ~IPropertyEditorBackend() = default;

    /// Construct an editor for the supplied descriptor.
    /// Returning nullptr is allowed and signals "this backend cannot
    /// edit that type"; the factory propagates that decision.
    [[nodiscard]] virtual std::unique_ptr<IPropertyEditor>
        Create(const PropertyDescriptor& descriptor) = 0;

    /// Capability query the factory exposes to consumers so they can
    /// decide whether to register an editor at all. Defaults to true
    /// for every primitive — backends override this when a specific
    /// type is unavailable in the current build.
    [[nodiscard]] virtual bool Supports(PropertyType /*type*/) const noexcept
    {
        return true;
    }
};

} // namespace SagaEditor
