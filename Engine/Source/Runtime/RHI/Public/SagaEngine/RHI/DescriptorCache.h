/// @file DescriptorCache.h
/// @brief Caches descriptor sets / root signatures to avoid per-draw creation.

#pragma once

#include "SagaEngine/RHI/Types/BufferTypes.h"
#include "SagaEngine/RHI/Types/TextureTypes.h"

#include <cstdint>
#include <unordered_map>

namespace SagaEngine::RHI
{

/// A descriptor binding — one buffer or texture view bound to a shader slot.
struct DescriptorBinding
{
    std::uint32_t    slot   = 0;
    BufferHandle     buffer = BufferHandle::kInvalid;
    TextureViewHandle texture = TextureViewHandle::kInvalid;
};

/// Key for caching a set of bindings.  Hash is computed from the slots + handles.
struct DescriptorSetKey
{
    std::uint64_t hash = 0;
    bool operator==(const DescriptorSetKey& o) const noexcept { return hash == o.hash; }
};

struct DescriptorSetKeyHash
{
    std::size_t operator()(const DescriptorSetKey& k) const noexcept
    { return static_cast<std::size_t>(k.hash); }
};

/// Opaque handle to a cached descriptor set.
enum class DescriptorSetHandle : std::uint32_t { kInvalid = 0 };

/// Simple cache mapping binding combinations to pre-built descriptor sets.
/// The backend fills this during material creation; Submit looks up
/// existing sets by hash to avoid re-creating them every frame.
class DescriptorCache
{
public:
    void Clear() { m_cache.clear(); }

    [[nodiscard]] DescriptorSetHandle Find(const DescriptorSetKey& key) const
    {
        auto it = m_cache.find(key);
        return (it != m_cache.end()) ? it->second : DescriptorSetHandle::kInvalid;
    }

    void Insert(const DescriptorSetKey& key, DescriptorSetHandle set)
    {
        m_cache[key] = set;
    }

    [[nodiscard]] std::size_t Size() const noexcept { return m_cache.size(); }

private:
    std::unordered_map<DescriptorSetKey, DescriptorSetHandle, DescriptorSetKeyHash> m_cache;
};

} // namespace SagaEngine::RHI
