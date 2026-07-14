/// @file ComponentEditorRegistry.h
/// @brief Type-keyed factory map that produces inspector panels per component.

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace SagaEditor
{

class IPanel;

// ─── Editor Descriptor ────────────────────────────────────────────────────────

/// Descriptor returned by the registry for diagnostics and UI generation.
/// Holds the human-readable display name shown in the inspector header
/// and the std::type_index the editor was registered against.
struct ComponentEditorDescriptor
{
    std::type_index componentType;       ///< std::type_index keying this editor.
    std::string     displayName;         ///< Header label shown in the inspector.
};

// ─── Component Editor Registry ────────────────────────────────────────────────

/// Maps a component's `std::type_index` to a factory that produces a
/// dockable `IPanel` showing that component's editable fields. The
/// inspector queries this registry every time the active selection
/// changes, so factories must be cheap and side-effect-free; expensive
/// state (icons, undo handles, asset previews) belongs inside the
/// produced panel, not the factory closure.
class ComponentEditorRegistry
{
public:
    /// Stateless functor that produces a fresh editor panel on demand.
    /// Ownership transfers to the caller via `unique_ptr`.
    using FactoryFn = std::function<std::unique_ptr<IPanel>()>;

    ComponentEditorRegistry()  = default;
    ~ComponentEditorRegistry() = default;

    ComponentEditorRegistry(const ComponentEditorRegistry&)            = delete;
    ComponentEditorRegistry& operator=(const ComponentEditorRegistry&) = delete;
    ComponentEditorRegistry(ComponentEditorRegistry&&)                 = default;
    ComponentEditorRegistry& operator=(ComponentEditorRegistry&&)      = default;

    // ─── Registration ─────────────────────────────────────────────────────────

    /// Register an editor factory for a component type.
    /// Re-registering the same type replaces the previous factory and
    /// the previous display name; the registry never silently merges.
    void Register(std::type_index componentType,
                  std::string     displayName,
                  FactoryFn       factory);

    /// Type-deduced overload that resolves the std::type_index from the
    /// component template parameter so call sites do not have to repeat
    /// `typeid(T)`.
    template <typename TComponent>
    void Register(std::string displayName, FactoryFn factory)
    {
        Register(std::type_index(typeid(TComponent)),
                 std::move(displayName),
                 std::move(factory));
    }

    /// Remove the editor for a component type, if any. Returns true if
    /// an entry was removed; false if no editor was registered.
    bool Unregister(std::type_index componentType);

    /// Drop every registered editor. Used by the test harness and by
    /// the editor shell during teardown.
    void Clear() noexcept;

    // ─── Lookup ───────────────────────────────────────────────────────────────

    /// Return true when an editor is registered for `componentType`.
    [[nodiscard]] bool Has(std::type_index componentType) const noexcept;

    /// Build a fresh editor panel for `componentType`.
    /// Returns `nullptr` when no factory is registered, or when the
    /// factory itself returns `nullptr` (which is treated as a soft
    /// "no editor right now" rather than an error so packages can
    /// gate visibility on platform).
    [[nodiscard]] std::unique_ptr<IPanel>
        Create(std::type_index componentType) const;

    /// Look up the descriptor for `componentType`, or `nullptr` when
    /// no editor is registered. Pointer stays valid until the matching
    /// `Unregister` or `Clear` call.
    [[nodiscard]] const ComponentEditorDescriptor*
        Find(std::type_index componentType) const noexcept;

    /// Number of currently registered editors.
    [[nodiscard]] std::size_t Size() const noexcept;

    /// Snapshot of every registered descriptor. Returned by value so
    /// callers cannot hold a reference into the internal table across
    /// later registrations.
    [[nodiscard]] std::vector<ComponentEditorDescriptor>
        Descriptors() const;

private:
    // ─── Internal Storage ─────────────────────────────────────────────────────

    /// Per-entry record. `descriptor.componentType` is the canonical
    /// key and matches the map slot.
    struct Entry
    {
        ComponentEditorDescriptor descriptor; ///< Display name + type index.
        FactoryFn                 factory;    ///< Stateless editor factory.
    };

    std::unordered_map<std::type_index, Entry> m_entries; ///< Keyed by type index.
};

} // namespace SagaEditor
