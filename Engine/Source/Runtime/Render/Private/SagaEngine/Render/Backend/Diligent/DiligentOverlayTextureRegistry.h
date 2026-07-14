/// @file DiligentOverlayTextureRegistry.h
/// @brief Private overlay texture translation registry.

#pragma once

#include "SagaEngine/Graphics/Handles/GraphicsHandle.h"
#include "SagaEngine/Render/Backend/RenderBackendFactory.h"

#include <cstdint>
#include <vector>

namespace SagaEngine::Render::Backend
{

[[nodiscard]] bool ValidateOverlayFrameForTests(
    const RenderOverlayFrame& frame) noexcept;

class DiligentOverlayTextureRegistry final
{
public:
    enum class SlotState : std::uint8_t
    {
        Free,
        Ready,
    };

    struct DestroyResult
    {
        bool destroyed = false;
        Graphics::TextureHandle nativeTexture{};
    };

    [[nodiscard]] RenderOverlayTextureHandle Create(
        Graphics::TextureHandle nativeTexture);
    [[nodiscard]] DestroyResult Destroy(
        RenderOverlayTextureHandle texture) noexcept;
    [[nodiscard]] Graphics::TextureHandle Resolve(
        RenderOverlayTextureHandle texture) const noexcept;
    [[nodiscard]] std::vector<Graphics::TextureHandle> DestroyAll();

    [[nodiscard]] static std::uint32_t AdvanceGeneration(
        std::uint32_t generation) noexcept;

    [[nodiscard]] std::size_t SlotCountForTests() const noexcept
    {
        return m_slots.size();
    }
    [[nodiscard]] std::size_t FreeCountForTests() const noexcept
    {
        return m_freeSlots.size();
    }
    [[nodiscard]] std::uint32_t GenerationForTests(
        std::uint32_t publicIndex) const noexcept;
    void SetGenerationForTests(
        std::uint32_t publicIndex,
        std::uint32_t generation);

private:
    struct Slot
    {
        SlotState state = SlotState::Free;
        std::uint32_t generation = 1u;
        Graphics::TextureHandle nativeTexture{};
    };

    std::vector<Slot> m_slots;
    std::vector<std::uint32_t> m_freeSlots;
};

} // namespace SagaEngine::Render::Backend
