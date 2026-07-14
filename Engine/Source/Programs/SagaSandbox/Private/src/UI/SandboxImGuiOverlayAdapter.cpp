/// @file SandboxImGuiOverlayAdapter.cpp
/// @brief Dear ImGui to Saga generic overlay draw-list conversion.

#include "SagaSandbox/UI/SandboxImGuiOverlayAdapter.h"
#include "SagaSandbox/UI/SandboxImGuiOverlayToken.h"

#include <imgui.h>

#include <atomic>
#include <cstdint>
#include <cstdio>

namespace SagaSandbox
{

namespace RB = SagaEngine::Render::Backend;

namespace
{

std::atomic<std::uint32_t> g_nextAdapterMagic{1u};

void LogOverlayTokenDiagnostic(const char* message)
{
    std::fprintf(stderr, "[SandboxImGuiOverlay][diagnostic] %s\n", message);
    std::fflush(stderr);
}

[[nodiscard]] ImTextureID TokenToImTextureID(std::uintptr_t token) noexcept
{
    return Detail::OverlayTokenToImTextureID<ImTextureID>(token);
}

[[nodiscard]] std::uintptr_t ImTextureIDToToken(ImTextureID texture) noexcept
{
    return Detail::ImTextureIDToOverlayToken(texture);
}

} // namespace

SandboxImGuiOverlayAdapter::SandboxImGuiOverlayAdapter()
{
    m_instanceMagic = g_nextAdapterMagic.fetch_add(1u);
    m_instanceMagic &= 0xFFFFu;
    if (m_instanceMagic == 0u)
    {
        m_instanceMagic = 1u;
    }
}

bool SandboxImGuiOverlayAdapter::Initialize(RB::IRenderBackend& backend)
{
    if (m_ready)
    {
        return true;
    }
    if (!RB::InitBackendOverlayRendering(backend))
    {
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    RB::RenderOverlayTextureDesc desc{};
    desc.width = static_cast<std::uint32_t>(width);
    desc.height = static_cast<std::uint32_t>(height);
    desc.rowPitchBytes = static_cast<std::uint32_t>(width) * 4u;

    m_fontTexture = RB::CreateBackendOverlayTexture(
        backend, desc, pixels);
    if (!m_fontTexture)
    {
        RB::ShutdownBackendOverlayRendering(backend);
        return false;
    }

    m_fontToken = CreateToken(m_fontTexture);
    io.Fonts->SetTexID(TokenToImTextureID(m_fontToken));

    m_ready = true;
    return true;
}

void SandboxImGuiOverlayAdapter::Shutdown(RB::IRenderBackend& backend)
{
    ImGui::GetIO().Fonts->SetTexID({});
    DestroyToken(m_fontToken);
    m_fontToken = 0;

    if (m_fontTexture)
    {
        RB::DestroyBackendOverlayTexture(backend, m_fontTexture);
        m_fontTexture = {};
    }

    InvalidateAllTokens();
    m_tokens.clear();
    RB::ShutdownBackendOverlayRendering(backend);
    m_ready = false;
}

void SandboxImGuiOverlayAdapter::Render(
    RB::IRenderBackend& backend,
    const ImDrawData* drawData)
{
    if (!m_ready || !drawData || drawData->TotalVtxCount <= 0)
    {
        return;
    }

    RB::RenderOverlayFrame frame{};
    frame.displayPos[0] = drawData->DisplayPos.x;
    frame.displayPos[1] = drawData->DisplayPos.y;
    frame.displaySize[0] = drawData->DisplaySize.x;
    frame.displaySize[1] = drawData->DisplaySize.y;
    frame.framebufferScale[0] = drawData->FramebufferScale.x;
    frame.framebufferScale[1] = drawData->FramebufferScale.y;
    frame.drawLists.reserve(static_cast<std::size_t>(drawData->CmdListsCount));

    for (int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex)
    {
        const ImDrawList* src = drawData->CmdLists[listIndex];
        RB::RenderOverlayDrawList dst{};

        dst.vertices.reserve(static_cast<std::size_t>(src->VtxBuffer.Size));
        for (int vertexIndex = 0; vertexIndex < src->VtxBuffer.Size;
             ++vertexIndex)
        {
            const ImDrawVert& in = src->VtxBuffer[vertexIndex];
            RB::RenderOverlayVertex out{};
            out.pos[0] = in.pos.x;
            out.pos[1] = in.pos.y;
            out.uv[0] = in.uv.x;
            out.uv[1] = in.uv.y;
            out.colorRgba8 = in.col;
            dst.vertices.push_back(out);
        }

        dst.indices.reserve(static_cast<std::size_t>(src->IdxBuffer.Size));
        for (int indexIndex = 0; indexIndex < src->IdxBuffer.Size;
             ++indexIndex)
        {
            dst.indices.push_back(
                static_cast<std::uint32_t>(src->IdxBuffer[indexIndex]));
        }

        dst.commands.reserve(static_cast<std::size_t>(src->CmdBuffer.Size));
        for (int commandIndex = 0; commandIndex < src->CmdBuffer.Size;
             ++commandIndex)
        {
            const ImDrawCmd& in = src->CmdBuffer[commandIndex];
            if (in.UserCallback)
            {
                in.UserCallback(src, &in);
                continue;
            }

            const auto* binding =
                ResolveToken(ImTextureIDToToken(in.GetTexID()));
            if (!binding || !binding->handle)
            {
                continue;
            }

            RB::RenderOverlayDrawCommand out{};
            out.texture = binding->handle;
            out.clipRect.left = in.ClipRect.x;
            out.clipRect.top = in.ClipRect.y;
            out.clipRect.right = in.ClipRect.z;
            out.clipRect.bottom = in.ClipRect.w;
            out.indexOffset = static_cast<std::uint32_t>(in.IdxOffset);
            out.vertexOffset = static_cast<std::uint32_t>(in.VtxOffset);
            out.elementCount = static_cast<std::uint32_t>(in.ElemCount);
            dst.commands.push_back(out);
        }

        frame.drawLists.push_back(std::move(dst));
    }

    RB::RenderBackendOverlayFrame(backend, frame);
}

std::uintptr_t SandboxImGuiOverlayAdapter::CreateToken(
    RB::RenderOverlayTextureHandle handle)
{
    if (!handle)
    {
        return 0;
    }

    for (const auto& token : m_tokens)
    {
        if (token.live && token.handle == handle)
        {
            return Detail::PackOverlayToken(
                m_instanceMagic, token.generation, token.id);
        }
    }

    for (auto& token : m_tokens)
    {
        if (!token.live)
        {
            token.handle = handle;
            token.generation =
                Detail::AdvanceOverlayTokenGeneration(token.generation);
            token.live = true;
            return Detail::PackOverlayToken(
                m_instanceMagic, token.generation, token.id);
        }
    }

    if (m_nextTokenId > Detail::OverlayTokenMaxId())
    {
        return 0;
    }

    TextureToken token{};
    token.handle = handle;
    token.id = m_nextTokenId++;
    token.generation = 1u;
    token.live = true;
    m_tokens.push_back(token);
    return Detail::PackOverlayToken(m_instanceMagic, token.generation, token.id);
}

const SandboxImGuiOverlayAdapter::TextureToken*
SandboxImGuiOverlayAdapter::ResolveToken(std::uintptr_t texture) const
{
    if (texture == 0u)
    {
        LogOverlayTokenDiagnostic("null ImGui texture token skipped");
        return nullptr;
    }
    if (Detail::OverlayTokenMagic(texture) != m_instanceMagic)
    {
        LogOverlayTokenDiagnostic("foreign ImGui texture token skipped");
        return nullptr;
    }

    const auto id = Detail::OverlayTokenId(texture);
    const auto generation = Detail::OverlayTokenGeneration(texture);
    for (const auto& token : m_tokens)
    {
        if (token.id == id)
        {
            if (!token.live || token.generation != generation)
            {
                LogOverlayTokenDiagnostic("stale ImGui texture token skipped");
                return nullptr;
            }
            return &token;
        }
    }

    LogOverlayTokenDiagnostic("unknown ImGui texture token skipped");
    return nullptr;
}

void SandboxImGuiOverlayAdapter::DestroyToken(std::uintptr_t texture) noexcept
{
    if (texture == 0u || Detail::OverlayTokenMagic(texture) != m_instanceMagic)
    {
        return;
    }

    const auto id = Detail::OverlayTokenId(texture);
    const auto generation = Detail::OverlayTokenGeneration(texture);
    for (auto& token : m_tokens)
    {
        if (token.id == id && token.generation == generation)
        {
            token.live = false;
            token.handle = {};
            return;
        }
    }
}

void SandboxImGuiOverlayAdapter::InvalidateAllTokens() noexcept
{
    for (auto& token : m_tokens)
    {
        token.live = false;
        token.generation =
            Detail::AdvanceOverlayTokenGeneration(token.generation);
        token.handle = {};
    }
}

} // namespace SagaSandbox
