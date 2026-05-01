/// @file IPropertyEditor.h
/// @brief Framework-free contract every primitive property editor implements.

#pragma once

#include "SagaEditor/InspectorEditing/PropertyEditorFactory.h"
#include "SagaEditor/Panels/IPanel.h"

#include <functional>
#include <string>
#include <string_view>

namespace SagaEditor
{

// ─── Property Value Codec ─────────────────────────────────────────────────────

/// Every primitive editor exchanges values with the inspector binder
/// as a serialised string. The encoding is per-`PropertyType` and is
/// documented next to each enumerator below; the binder decodes the
/// string into the consumer's own schema. Using strings here lets the
/// editor talk to the binder without leaking any math types, asset
/// types, or component types into the IPropertyEditor contract.
///
///   Bool   ─ "true" / "false" (lowercase, no whitespace).
///   Int    ─ signed decimal, optional leading "-".
///   Float  ─ standard decimal, optional exponent (`1e3`, `-2.5`).
///   String ─ raw text, no escaping.
///   Vec2   ─ "x,y"      decimal floats, comma-separated, no spaces.
///   Vec3   ─ "x,y,z"    decimal floats, comma-separated, no spaces.
///   Vec4   ─ "x,y,z,w"  decimal floats, comma-separated, no spaces.
///   Quat   ─ Euler degrees "pitch,yaw,roll", same comma convention.
///   Color  ─ "#RRGGBBAA" 8 hex digits, capital letters, no leading "0x".
///   Enum   ─ stable enum value name (case-sensitive).
///   Asset  ─ "kind:assetId" — kind is a stable lowercase token.

// ─── Property Editor Interface ────────────────────────────────────────────────

/// Base interface for every primitive editor produced by the
/// `PropertyEditorFactory`. Editors stay framework-free at the public
/// surface — concrete Qt widgets live behind a pimpl in the backend
/// implementation. The `IPanel` base lets the inspector embed the
/// editor inside its scroll area through the same docking primitives
/// every other panel uses.
class IPropertyEditor : public IPanel
{
public:
    /// Notification signature for value commits. The string `value`
    /// follows the per-type encoding documented above.
    using ValueChangedFn = std::function<void(const std::string& value)>;

    ~IPropertyEditor() override = default;

    // ─── Identity ─────────────────────────────────────────────────────────────

    /// The primitive type this editor edits. Stable for the lifetime
    /// of the editor instance.
    [[nodiscard]] virtual PropertyType GetPropertyType() const noexcept = 0;

    /// User-visible label shown next to the editor widget.
    [[nodiscard]] virtual const std::string& GetLabel() const noexcept = 0;

    /// Dot-separated binding path the editor was constructed against.
    /// The inspector binder forwards this verbatim to its subscribers.
    [[nodiscard]] virtual const std::string& GetBindingPath() const noexcept = 0;

    // ─── Value Access ─────────────────────────────────────────────────────────

    /// Pull the current value as its string-encoded form. Used by the
    /// inspector to serialise the editor's transient state into undo
    /// records and into multi-edit divergent-value markers.
    [[nodiscard]] virtual std::string GetValueAsString() const = 0;

    /// Push a new value into the editor without firing the
    /// `OnValueChanged` callback. Used by the inspector to sync the
    /// widget after an undo, an external edit from a collaborator, or
    /// a multi-edit reset. Returns false when the input fails to
    /// parse against the per-type encoding.
    virtual bool SetValueFromString(std::string_view value) = 0;

    // ─── Change Notification ──────────────────────────────────────────────────

    /// Install the callback the editor invokes after the user commits
    /// an edit (loses focus, presses Enter, releases a drag handle).
    /// The previous callback, if any, is replaced. An empty
    /// `std::function` clears the binding.
    virtual void SetOnValueChanged(ValueChangedFn callback) = 0;

    // ─── Read-Only Mode ───────────────────────────────────────────────────────

    /// Toggle the read-only state. Editors render the disabled style
    /// and ignore user input while read-only.
    virtual void SetReadOnly(bool readOnly) noexcept = 0;

    /// Current read-only flag.
    [[nodiscard]] virtual bool IsReadOnly() const noexcept = 0;
};

} // namespace SagaEditor
