/// @file EditorProfileRegistry.h
/// @brief Catalogue and active-state tracking for editor workflow profiles.

#pragma once

#include "SagaEditor/Profile/EditorProfile.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEditor
{

using EditorProfileChangeSubscription = std::uint64_t;
inline constexpr EditorProfileChangeSubscription kInvalidEditorProfileSubscription = 0;

class EditorProfileRegistry
{
public:
    using ProfileChangedFn = std::function<void(const EditorProfile&)>;

    EditorProfileRegistry();
    ~EditorProfileRegistry() = default;

    EditorProfileRegistry(const EditorProfileRegistry&) = delete;
    EditorProfileRegistry& operator=(const EditorProfileRegistry&) = delete;
    EditorProfileRegistry(EditorProfileRegistry&&) = default;
    EditorProfileRegistry& operator=(EditorProfileRegistry&&) = default;

    void RegisterBuiltinProfiles();
    bool Register(EditorProfile profile);
    bool Unregister(const std::string& id);
    void Clear() noexcept;

    [[nodiscard]] bool Has(const std::string& id) const;
    [[nodiscard]] std::size_t Size() const noexcept;
    [[nodiscard]] const EditorProfile* Find(const std::string& id) const;
    [[nodiscard]] std::vector<EditorProfile> GetAll() const;

    bool SetActive(const std::string& id);
    [[nodiscard]] const EditorProfile* GetActive() const noexcept;
    [[nodiscard]] const std::string& GetActiveId() const noexcept;

    EditorProfileChangeSubscription OnProfileChanged(ProfileChangedFn callback);
    bool RemoveProfileChangedCallback(EditorProfileChangeSubscription handle) noexcept;
    [[nodiscard]] std::size_t SubscriberCount() const noexcept;

private:
    struct Subscriber
    {
        EditorProfileChangeSubscription handle = kInvalidEditorProfileSubscription;
        ProfileChangedFn callback;
    };

    std::unordered_map<std::string, EditorProfile> m_profiles;
    std::string m_activeId;
    std::uint64_t m_nextHandle = kInvalidEditorProfileSubscription + 1;
    std::vector<Subscriber> m_subscribers;
};

} // namespace SagaEditor
