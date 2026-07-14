/// @file DiligentRenderBackendResources.cpp
/// @brief Mesh, material, and texture resource upload for DiligentRenderBackend.

#include "SagaEngine/Render/Backend/Diligent/DiligentRenderBackendPrivate.h"

namespace SagaEngine::Render::Backend
{

World::MeshId DiligentRenderBackend::CreateMesh(const MeshAsset& asset)
{
    if (!m_Impl->runtime->IsInitialized() || !m_Impl->runtime->Device()) return World::MeshId::kInvalid;
    if (asset.lodCount == 0) return World::MeshId::kInvalid;

    const auto& lod = asset.lods[0];
    if (lod.vertices.empty()) return World::MeshId::kInvalid;

    MeshGPU gpu{};
    gpu.vertexStride = static_cast<Diligent::Uint32>(sizeof(MeshVertex));
    gpu.vertexCount  = static_cast<Diligent::Uint32>(lod.vertices.size());

    // VB
    {
        Gfx::BufferDesc vbDesc;
        vbDesc.debugName = "MeshVB";
        vbDesc.sizeBytes =
            static_cast<std::uint64_t>(gpu.vertexCount) * gpu.vertexStride;
        vbDesc.usage = Gfx::BufferUsage::Vertex;

        const Gfx::GraphicsDataView vbData{
            lod.vertices.data(),
            vbDesc.sizeBytes,
            0u,
            0u,
        };

        gpu.vertexBuffer =
            m_Impl->runtime->NativeResources().CreateStandaloneBuffer(vbDesc, vbData);
        if (!gpu.vertexBuffer.IsValid())
        {
            LOG_CAT_ERROR(Render, "CreateMesh: VB creation failed");
            return World::MeshId::kInvalid;
        }
    }

    // IB
    if (!lod.indices.empty())
    {
        gpu.indexCount = static_cast<Diligent::Uint32>(lod.indices.size());

        Gfx::BufferDesc ibDesc;
        ibDesc.debugName = "MeshIB";
        ibDesc.sizeBytes =
            static_cast<std::uint64_t>(gpu.indexCount) * sizeof(std::uint32_t);
        ibDesc.usage = Gfx::BufferUsage::Index;

        const Gfx::GraphicsDataView ibData{
            lod.indices.data(),
            ibDesc.sizeBytes,
            0u,
            0u,
        };

        gpu.indexBuffer =
            m_Impl->runtime->NativeResources().CreateStandaloneBuffer(ibDesc, ibData);
        if (!gpu.indexBuffer.IsValid())
        {
            LOG_CAT_ERROR(Render, "CreateMesh: IB creation failed");
            m_Impl->runtime->NativeResources().DestroyBuffer(gpu.vertexBuffer);
            return World::MeshId::kInvalid;
        }
    }

    auto id = static_cast<World::MeshId>(m_Impl->nextMeshId++);
    m_Impl->meshCache[id] = std::move(gpu);
    return id;
}

World::MaterialId DiligentRenderBackend::CreateMaterial(const MaterialRuntime& runtime)
{
    if (!m_Impl->runtime->IsInitialized() || !m_Impl->runtime->Device()) return World::MaterialId::kInvalid;

    PSOCacheKey key{};
    key.cullMode    = static_cast<std::uint8_t>(runtime.cullMode);
    key.renderQueue = static_cast<std::uint8_t>(runtime.renderQueue);
    key.writesDepth = runtime.writesDepth;

    auto pso = FindOrCreatePSO(m_Impl->psoCache, *m_Impl->runtime->Device(), *m_Impl->runtime->SwapChain(),
                                m_Impl->solidVS, m_Impl->solidPS, m_Impl->cameraCB, key);
    if (!pso) return World::MaterialId::kInvalid;

    using namespace Diligent;

    MaterialGPU gpu{};
    gpu.pso         = pso;
    gpu.renderQueue = runtime.renderQueue;
    gpu.cullMode    = runtime.cullMode;
    gpu.shadingModel = runtime.shadingModel;
    gpu.writesDepth = runtime.writesDepth;
    gpu.receivesShadows = runtime.receivesShadows;
    gpu.castsShadows = runtime.castsShadows;
    gpu.albedoTex   = runtime.textures[static_cast<std::size_t>(MaterialTextureSlot::Albedo)];

    gpu.binding = m_Impl->runtime->CreateMaterialBinding(*pso);
    if (!gpu.binding.IsValid())
    {
        LOG_CAT_ERROR(Render, "CreateMaterial: SRB creation failed");
        return World::MaterialId::kInvalid;
    }

    auto id = static_cast<World::MaterialId>(m_Impl->nextMaterialId++);
    m_Impl->materialCache[id] = std::move(gpu);
    return id;
}

void DiligentRenderBackend::DestroyMesh(World::MeshId id)
{
    auto it = m_Impl->meshCache.find(id);
    if (it != m_Impl->meshCache.end())
    {
        m_Impl->runtime->NativeResources().DestroyBuffer(it->second.vertexBuffer);
        m_Impl->runtime->NativeResources().DestroyBuffer(it->second.indexBuffer);
        m_Impl->meshCache.erase(it);
    }
}

void DiligentRenderBackend::DestroyMaterial(World::MaterialId id)
{
    auto it = m_Impl->materialCache.find(id);
    if (it == m_Impl->materialCache.end())
    {
        return;
    }

    m_Impl->runtime->DestroyMaterialBinding(it->second.binding);
    m_Impl->runtime->DestroyMaterialBinding(it->second.skinnedBinding);
    m_Impl->materialCache.erase(it);
}

// ─── Texture upload ─────────────────────────────────────────────────

TextureHandle DiligentRenderBackend::CreateTexture(uint32_t width, uint32_t height,
                                                    const uint8_t* rgba)
{
    if (!m_Impl->runtime->IsInitialized() || !m_Impl->runtime->Device() || !rgba)
        return TextureHandle::kInvalid;
    if (width == 0 || height == 0)
        return TextureHandle::kInvalid;

    Gfx::TextureDesc texDesc;
    texDesc.debugName = "UserTexture";
    texDesc.width = width;
    texDesc.height = height;
    texDesc.format = Gfx::ResourceFormat::Rgba8Unorm;

    const Gfx::GraphicsDataView texData{
        rgba,
        static_cast<std::uint64_t>(width) * height * 4u,
        width * 4u,
        0u,
    };

    TextureGPU gpu{};
    gpu.texture =
        m_Impl->runtime->NativeResources().CreateStandaloneTexture(texDesc, texData);
    if (!gpu.texture.IsValid())
    {
        LOG_CAT_ERROR(Render, "CreateTexture: texture creation failed");
        return TextureHandle::kInvalid;
    }

    auto handle = static_cast<TextureHandle>(m_Impl->nextTextureId++);
    m_Impl->textureCache[handle] = std::move(gpu);

    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "CreateTexture: %ux%u → handle %u",
                      width, height, static_cast<unsigned>(handle));
        LOG_CAT_DEBUG(Render, buf);
    }

    return handle;
}

void DiligentRenderBackend::DestroyTexture(TextureHandle tex)
{
    auto it = m_Impl->textureCache.find(tex);
    if (it != m_Impl->textureCache.end())
    {
        m_Impl->runtime->NativeResources().DestroyTexture(it->second.texture);
        m_Impl->textureCache.erase(it);
    }
}

// ─── Frame lifecycle ─────────────────────────────────────────────────

} // namespace SagaEngine::Render::Backend
