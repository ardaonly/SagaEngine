/// @file SandboxImGuiOverlayAdapter.h
/// @brief Converts Dear ImGui draw data into Saga's generic overlay API.

#pragma once

#include <SagaEngine/Render/Backend/RenderBackendFactory.h>

#include <cstdint>
#include <vector>

struct ImDrawData;

namespace SagaSandbox
{

class SandboxImGuiOverlayAdapter final
{
public:
    SandboxImGuiOverlayAdapter();
    ~SandboxImGuiOverlayAdapter() = default;

    SandboxImGuiOverlayAdapter(const SandboxImGuiOverlayAdapter&) = delete;
    SandboxImGuiOverlayAdapter& operator=(
        const SandboxImGuiOverlayAdapter&) = delete;

    [[nodiscard]] bool Initialize(
        SagaEngine::Render::Backend::IRenderBackend& backend);
    void Shutdown(SagaEngine::Render::Backend::IRenderBackend& backend);
    void Render(
        SagaEngine::Render::Backend::IRenderBackend& backend,
        const ImDrawData* drawData);

    [[nodiscard]] bool IsReady() const noexcept { return m_ready; }

private:
    struct TextureToken
    {
        SagaEngine::Render::Backend::RenderOverlayTextureHandle handle{};
        std::uint32_t id = 0;
        std::uint32_t generation = 1;
        bool live = false;
    };

    [[nodiscard]] std::uintptr_t CreateToken(
        SagaEngine::Render::Backend::RenderOverlayTextureHandle handle);
    [[nodiscard]] const TextureToken* ResolveToken(
        std::uintptr_t texture) const;
    void DestroyToken(std::uintptr_t texture) noexcept;
    void InvalidateAllTokens() noexcept;

    bool m_ready = false;
    SagaEngine::Render::Backend::RenderOverlayTextureHandle m_fontTexture{};
    std::uintptr_t m_fontToken = 0;
    std::uint32_t m_instanceMagic = 0;
    std::uint32_t m_nextTokenId = 1;
    std::vector<TextureToken> m_tokens;
};

} // namespace SagaSandbox
