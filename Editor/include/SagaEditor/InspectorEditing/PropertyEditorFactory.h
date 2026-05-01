/// @file PropertyEditorFactory.h
/// @brief Builds primitive-typed property editors for the inspector panel.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace SagaEditor
{

class IPanel;
class IPropertyEditorBackend;

// ─── Property Type Enumeration ────────────────────────────────────────────────

/// Every primitive type the inspector knows how to render natively.
/// Component editors that need a richer widget (asset reference popup,
/// curve editor, gradient picker) register their own editors through
/// `ComponentEditorRegistry` instead of going through this factory.
enum class PropertyType : std::uint8_t
{
    Bool,    ///< Single check box.
    Int,     ///< Signed 64-bit integer with drag-to-modify spinner.
    Float,   ///< Double-precision spinner with optional unit suffix.
    String,  ///< Single-line text edit.
    Vec2,    ///< Two-component float row.
    Vec3,    ///< Three-component float row.
    Vec4,    ///< Four-component float row.
    Quat,    ///< Quaternion shown as Euler angles in degrees.
    Color,   ///< RGBA colour picker.
    Enum,    ///< Drop-down driven by an external enum descriptor.
    Asset,   ///< Asset-reference picker filtered by `AssetKind`.
};

// ─── Property Descriptor ──────────────────────────────────────────────────────

/// Static description of a single editable property. The descriptor
/// is consumed by `PropertyEditorFactory::Create` and may be cached
/// across selection changes; instances are immutable after creation.
struct PropertyDescriptor
{
    PropertyType type        = PropertyType::String; ///< Primitive shape.
    std::string  label;                              ///< User-visible label.
    std::string  bindingPath;                        ///< Dot-separated path.
    std::string  toolTip;                            ///< Optional tool tip.
    std::string  unitSuffix;                         ///< e.g. "m", "deg".
    bool         readOnly    = false;                ///< Disable editing.

    /// Helper that constructs a descriptor with the most common shape
    /// (label + binding path + type) without forcing every call site
    /// to use designated initialisers.
    [[nodiscard]] static PropertyDescriptor
        Make(PropertyType    type,
             std::string_view label,
             std::string_view bindingPath);
};

// ─── Property Editor Factory ──────────────────────────────────────────────────

/// Stateless factory that produces an `IPanel` widget for a single
/// property. Today the factory returns the editor as a panel so the
/// inspector can lay it out using the same docking primitives every
/// other editor surface uses; this avoids one-off layout logic in
/// `InspectorPanel`. The factory is deliberately stateless because
/// produced editors should never share mutable state with the
/// factory itself — every editor binds its own copy of the property
/// path and does not need a back-reference.
class PropertyEditorFactory
{
public:
    PropertyEditorFactory()  = delete;
    ~PropertyEditorFactory() = delete;

    // ─── Public Construction ──────────────────────────────────────────────────

    /// Build an editor panel for the supplied descriptor.
    /// Returns `nullptr` when the requested type has no built-in
    /// editor (for example because a build configuration disabled
    /// asset references); the caller is expected to fall back to a
    /// read-only label widget in that case.
    [[nodiscard]] static std::unique_ptr<IPanel>
        Create(const PropertyDescriptor& descriptor);

    /// Convenience overload that builds a descriptor on the fly. The
    /// label and binding path are forwarded into the descriptor; the
    /// tool tip, unit suffix, and read-only flag stay at their
    /// defaults.
    [[nodiscard]] static std::unique_ptr<IPanel>
        Create(PropertyType     type,
               std::string_view label,
               std::string_view bindingPath);

    // ─── Capability Queries ───────────────────────────────────────────────────

    /// Return true when `Create` would produce a non-null editor for
    /// `type` in the current build. Used by the inspector to fall
    /// back to a read-only label without paying for an allocation.
    [[nodiscard]] static bool Supports(PropertyType type) noexcept;

    /// Stable, locale-independent identifier for `type`. Used by the
    /// layout serializer when persisting which editor instance was
    /// open in a saved layout.
    [[nodiscard]] static std::string_view TypeId(PropertyType type) noexcept;

    // ─── Backend Injection ────────────────────────────────────────────────────

    /// Install the backend the factory dispatches to. The editor host
    /// installs the Qt backend at start-up; tests can install a mock
    /// or pass nullptr to detach the previous backend (after which
    /// every `Create` call returns nullptr again). Ownership stays
    /// with the caller — the factory only stores a non-owning pointer.
    static void SetBackend(IPropertyEditorBackend* backend) noexcept;

    /// Currently installed backend, or nullptr when none has been
    /// installed. Mostly useful for tests; production callers should
    /// go through `Create`.
    [[nodiscard]] static IPropertyEditorBackend* GetBackend() noexcept;
};

} // namespace SagaEditor
