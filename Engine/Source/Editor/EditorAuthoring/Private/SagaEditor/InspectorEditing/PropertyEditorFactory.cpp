/// @file PropertyEditorFactory.cpp
/// @brief Stateless factory that produces inspector primitive editors.

#include "SagaEditor/InspectorEditing/PropertyEditorFactory.h"

#include "SagaEditor/InspectorEditing/IPropertyEditor.h"
#include "SagaEditor/InspectorEditing/IPropertyEditorBackend.h"

namespace SagaEditor
{

namespace
{

// ─── Backend Storage ──────────────────────────────────────────────────────────

/// Process-wide non-owning pointer to the currently installed backend.
/// `PropertyEditorFactory::SetBackend` is the only writer; access is
/// expected from the UI thread, so no synchronisation is required.
IPropertyEditorBackend* g_backend = nullptr;

} // namespace

// ─── Property Descriptor Helpers ──────────────────────────────────────────────

PropertyDescriptor PropertyDescriptor::Make(PropertyType     type,
                                            std::string_view label,
                                            std::string_view bindingPath)
{
    PropertyDescriptor desc;
    desc.type        = type;
    desc.label       = std::string(label);
    desc.bindingPath = std::string(bindingPath);
    return desc;
}

// ─── Construction ─────────────────────────────────────────────────────────────

std::unique_ptr<IPanel>
PropertyEditorFactory::Create(const PropertyDescriptor& descriptor)
{
    if (g_backend == nullptr)
    {
        // No backend installed — the inspector falls back to a
        // read-only label. This is the default state in unit tests
        // and in headless tooling that never instantiates a Qt UI.
        return nullptr;
    }
    return g_backend->Create(descriptor);
}

std::unique_ptr<IPanel>
PropertyEditorFactory::Create(PropertyType     type,
                              std::string_view label,
                              std::string_view bindingPath)
{
    return Create(PropertyDescriptor::Make(type, label, bindingPath));
}

// ─── Capability Queries ───────────────────────────────────────────────────────

bool PropertyEditorFactory::Supports(PropertyType type) noexcept
{
    return g_backend != nullptr && g_backend->Supports(type);
}

std::string_view PropertyEditorFactory::TypeId(PropertyType type) noexcept
{
    // Stable string ids — used by the layout serializer; do not rename.
    switch (type)
    {
        case PropertyType::Bool:   return "saga.prop.bool";
        case PropertyType::Int:    return "saga.prop.int";
        case PropertyType::Float:  return "saga.prop.float";
        case PropertyType::String: return "saga.prop.string";
        case PropertyType::Vec2:   return "saga.prop.vec2";
        case PropertyType::Vec3:   return "saga.prop.vec3";
        case PropertyType::Vec4:   return "saga.prop.vec4";
        case PropertyType::Quat:   return "saga.prop.quat";
        case PropertyType::Color:  return "saga.prop.color";
        case PropertyType::Enum:   return "saga.prop.enum";
        case PropertyType::Asset:  return "saga.prop.asset";
    }
    return "saga.prop.unknown";
}

// ─── Backend Injection ────────────────────────────────────────────────────────

void PropertyEditorFactory::SetBackend(IPropertyEditorBackend* backend) noexcept
{
    g_backend = backend;
}

IPropertyEditorBackend* PropertyEditorFactory::GetBackend() noexcept
{
    return g_backend;
}

} // namespace SagaEditor
