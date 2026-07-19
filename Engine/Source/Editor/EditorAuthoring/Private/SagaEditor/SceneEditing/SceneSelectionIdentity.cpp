// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/SceneEditing/SceneSelectionIdentity.h"

namespace SagaEditor
{
SelectableId SceneSelectionId(std::string_view sceneId,
                              std::string_view entityId) noexcept
{
    constexpr std::uint64_t offset = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t hash = offset;
    const auto append = [&hash](std::string_view text)
    {
        for (const auto character : text)
        {
            hash ^= static_cast<unsigned char>(character);
            hash *= prime;
        }
    };
    append(sceneId);
    hash ^= 0xffU;
    hash *= prime;
    append(entityId);
    return hash == kInvalidSelectableId ? 1 : hash;
}
}
