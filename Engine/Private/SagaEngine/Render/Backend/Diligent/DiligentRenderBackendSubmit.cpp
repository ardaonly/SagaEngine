/// @file DiligentRenderBackendSubmit.cpp
/// @brief Scene submit and shadow pass execution for DiligentRenderBackend.

#include "SagaEngine/Render/Backend/Diligent/DiligentRenderBackendPrivate.h"
#include "SagaEngine/Graphics/Backends/Diligent/DiligentFallbackResources.h"

namespace SagaEngine::Render::Backend
{

namespace
{

struct NormalMatrixRows
{
    float rows[12]{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
    };
};

bool ComputeNormalMatrixRows(const ::SagaEngine::Math::Mat4& model,
                             NormalMatrixRows& outRows) noexcept
{
    const float a00 = model.At(0, 0), a01 = model.At(0, 1), a02 = model.At(0, 2);
    const float a10 = model.At(1, 0), a11 = model.At(1, 1), a12 = model.At(1, 2);
    const float a20 = model.At(2, 0), a21 = model.At(2, 1), a22 = model.At(2, 2);
    if (!std::isfinite(a00) || !std::isfinite(a01) || !std::isfinite(a02) ||
        !std::isfinite(a10) || !std::isfinite(a11) || !std::isfinite(a12) ||
        !std::isfinite(a20) || !std::isfinite(a21) || !std::isfinite(a22))
    {
        return false;
    }

    const float c00 =  (a11 * a22 - a12 * a21);
    const float c01 = -(a10 * a22 - a12 * a20);
    const float c02 =  (a10 * a21 - a11 * a20);
    const float c10 = -(a01 * a22 - a02 * a21);
    const float c11 =  (a00 * a22 - a02 * a20);
    const float c12 = -(a00 * a21 - a01 * a20);
    const float c20 =  (a01 * a12 - a02 * a11);
    const float c21 = -(a00 * a12 - a02 * a10);
    const float c22 =  (a00 * a11 - a01 * a10);

    const float det = a00 * c00 + a01 * c01 + a02 * c02;
    if (std::fabs(det) < 1.0e-6f)
        return false;

    const float invDet = 1.0f / det;

    // Rows of inverse-transpose. Translation is intentionally excluded.
    outRows.rows[0]  = c00 * invDet;
    outRows.rows[1]  = c01 * invDet;
    outRows.rows[2]  = c02 * invDet;
    outRows.rows[3]  = 0.0f;
    outRows.rows[4]  = c10 * invDet;
    outRows.rows[5]  = c11 * invDet;
    outRows.rows[6]  = c12 * invDet;
    outRows.rows[7]  = 0.0f;
    outRows.rows[8]  = c20 * invDet;
    outRows.rows[9]  = c21 * invDet;
    outRows.rows[10] = c22 * invDet;
    outRows.rows[11] = 0.0f;
    return true;
}

[[nodiscard]] bool NormalizeDirectionalLight(Scene::RenderLightingData& lighting) noexcept
{
    if (!lighting.directionalEnabled)
        return false;

    const float lenSq = lighting.directional.direction.LengthSq();
    if (lenSq < 1.0e-6f)
    {
        lighting.directionalEnabled = false;
        lighting.shadowsEnabled = false;
        return false;
    }

    lighting.directional.direction =
        lighting.directional.direction * (1.0f / std::sqrt(lenSq));
    return true;
}

[[nodiscard]] ::SagaEngine::Math::Mat4 BuildDirectionalLightViewProjection(
    const Scene::RenderLightingData& lighting,
    const DirectionalShadowSettings& settings) noexcept
{
    using ::SagaEngine::Math::Mat4;
    using ::SagaEngine::Math::Vec3;

    Vec3 dir = lighting.directional.direction;
    const float lenSq = dir.LengthSq();
    if (lenSq < 1.0e-6f)
        dir = {0.5f, -1.0f, 0.25f};
    dir = dir.Normalized();

    const Vec3 center{0.0f, 0.0f, 0.0f};
    const Vec3 eye = center - dir * (settings.farPlane * 0.5f);
    Vec3 up = Vec3::Up();
    if (std::fabs(dir.Dot(up)) > 0.95f)
        up = Vec3::Back();

    const Mat4 view = Mat4::LookAtRH(eye, center, up);
    const float e = settings.orthographicExtent;
    const Mat4 proj = Mat4::OrthoRH_ZO(
        -e, e, -e, e, settings.nearPlane, settings.farPlane);
    return proj * view;
}

} // anonymous namespace

void DiligentRenderBackend::Submit(const Scene::Camera&     camera,
                                   const Scene::RenderView& view)
{
    if (!m_Impl->runtime->IsInitialized() || !m_Impl->runtime->Context() || !m_Impl->cameraCB)
        return;

    auto* ctx = m_Impl->runtime->Context();
    const auto activeFrameSerial = m_Impl->runtime->FrameSlots().ActiveFrameSerial();

    // ── Nothing to draw? ─────────────────────────────────────────────
    if (view.drawItems.empty()) return;

    Scene::RenderLightingData lighting = view.lighting;
    const bool hasValidDirectional = NormalizeDirectionalLight(lighting);
    if (view.lighting.directionalEnabled && !hasValidDirectional)
    {
        LOG_CAT_DEBUG(Render, "Submit: zero-length directional light disabled");
    }

    const bool shadowRequested =
        lighting.directionalEnabled && lighting.shadowsEnabled;
    if (shadowRequested && m_Impl->shadowSubmitAcceptedThisFrame)
    {
        ++m_Impl->currentFrameDiagnostics.additionalFrameSubmitsRejected;
        m_Impl->currentFrameDiagnostics.rejectedDrawItems +=
            static_cast<std::uint32_t>(view.drawItems.size());
        LOG_CAT_DEBUG(Render, "Submit: additional shadow-enabled Submit rejected");
        return;
    }
    if (shadowRequested)
        m_Impl->shadowSubmitAcceptedThisFrame = true;

    {
        char msg[128];
        std::snprintf(msg, sizeof(msg), "Submit: %zu draw item(s) on frame %llu",
                      view.drawItems.size(),
                      static_cast<unsigned long long>(m_Impl->runtime->Status().frameIndex));
        LOG_CAT_DEBUG(Render, msg);
        if (m_Impl->runtime->Status().frameIndex == 0) LOG_CAT_INFO(Render, msg);
    }

    const float* vpData = camera.viewProj.Data();
    const auto lightViewProj =
        BuildDirectionalLightViewProjection(lighting, m_Impl->shadowSettings);
    const float* lightVpData = lightViewProj.Data();

    auto ensureShadowResources = [&]() -> bool
    {
        using namespace Diligent;

        const auto resolution = m_Impl->shadowSettings.resolution;
        if (resolution == 0)
            return false;

        const bool needsRecreate =
            !m_Impl->shadow.texture ||
            m_Impl->shadow.width != resolution ||
            m_Impl->shadow.height != resolution;

        if (needsRecreate)
        {
            if (m_Impl->shadow.texture)
            {
                ++m_Impl->shadow.recreationCount;
                ++m_Impl->currentFrameDiagnostics.shadowResourceRecreationCount;
                m_Impl->shadow.dsv.Release();
                m_Impl->shadow.srv.Release();
                m_Impl->shadow.texture.Release();
            }

            TextureDesc texDesc;
            texDesc.Name      = "DirectionalShadowMap";
            texDesc.Type      = RESOURCE_DIM_TEX_2D;
            texDesc.Width     = resolution;
            texDesc.Height    = resolution;
            texDesc.Format    = TEX_FORMAT_D32_FLOAT;
            texDesc.BindFlags = BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE;
            texDesc.Usage     = USAGE_DEFAULT;
            texDesc.MipLevels = 1;

            m_Impl->runtime->Device()->CreateTexture(
                texDesc, nullptr, &m_Impl->shadow.texture);
            if (!m_Impl->shadow.texture)
            {
                LOG_CAT_ERROR(Render, "Submit: failed to create directional shadow map");
                return false;
            }

            m_Impl->shadow.dsv = m_Impl->shadow.texture->GetDefaultView(
                TEXTURE_VIEW_DEPTH_STENCIL);
            m_Impl->shadow.srv = m_Impl->shadow.texture->GetDefaultView(
                TEXTURE_VIEW_SHADER_RESOURCE);
            if (!m_Impl->shadow.dsv || !m_Impl->shadow.srv)
            {
                LOG_CAT_ERROR(Render, "Submit: failed to create directional shadow views");
                m_Impl->shadow.dsv.Release();
                m_Impl->shadow.srv.Release();
                m_Impl->shadow.texture.Release();
                return false;
            }

            m_Impl->shadow.width = resolution;
            m_Impl->shadow.height = resolution;
            m_Impl->shadow.format = TEX_FORMAT_D32_FLOAT;
            ++m_Impl->shadow.creationCount;
            ++m_Impl->currentFrameDiagnostics.shadowResourceCreationCount;
        }

        if (!m_Impl->shadow.depthPSO)
        {
            m_Impl->shadow.depthPSO = CreateShadowDepthPSO(
                *m_Impl->runtime->Device(),
                m_Impl->shadow.depthVS,
                m_Impl->cameraCB,
                m_Impl->shadow.format);
            if (!m_Impl->shadow.depthPSO)
                return false;
        }

        m_Impl->currentFrameDiagnostics.shadowMapWidth = m_Impl->shadow.width;
        m_Impl->currentFrameDiagnostics.shadowMapHeight = m_Impl->shadow.height;
        return true;
    };

    auto uploadCameraCB = [&](const SagaEngine::Math::Mat4& model,
                              const NormalMatrixRows& normalRows,
                              float shadingModel,
                              bool shadowsEnabled)
    {
        Diligent::MapHelper<float> cbData(ctx, m_Impl->cameraCB,
                                          Diligent::MAP_WRITE,
                                          Diligent::MAP_FLAG_DISCARD);
        for (int i = 0; i < 16; ++i) cbData[i] = vpData[i];
        const float* mData = model.Data();
        for (int i = 0; i < 16; ++i) cbData[16 + i] = mData[i];
        for (int i = 0; i < 12; ++i) cbData[32 + i] = normalRows.rows[i];

        cbData[44] = lighting.directional.direction.x;
        cbData[45] = lighting.directional.direction.y;
        cbData[46] = lighting.directional.direction.z;
        cbData[47] = lighting.directionalEnabled ? 1.0f : 0.0f;

        cbData[48] = lighting.directional.color.x;
        cbData[49] = lighting.directional.color.y;
        cbData[50] = lighting.directional.color.z;
        cbData[51] = lighting.directional.intensity;

        cbData[52] = lighting.ambient.color.x;
        cbData[53] = lighting.ambient.color.y;
        cbData[54] = lighting.ambient.color.z;
        cbData[55] = lighting.ambient.intensity;

        for (int i = 0; i < 16; ++i) cbData[56 + i] = lightVpData[i];

        cbData[72] = m_Impl->shadowSettings.constantBias;
        cbData[73] = m_Impl->shadowSettings.normalBias;
        cbData[74] = m_Impl->shadow.width > 0
            ? 1.0f / static_cast<float>(m_Impl->shadow.width)
            : 1.0f / static_cast<float>(m_Impl->shadowSettings.resolution);
        cbData[75] = shadowsEnabled ? 1.0f : 0.0f;

        cbData[76] = shadingModel;
        cbData[77] = static_cast<float>(m_Impl->shadowSettings.pcfRadius);
        cbData[78] = 0.0f;
        cbData[79] = 0.0f;
    };

    auto bindMeshBuffers = [&](const MeshGPU& mesh,
                               Diligent::IBuffer*& lastVB,
                               Diligent::IBuffer*& lastIB)
    {
        auto* vb = m_Impl->runtime->NativeResources().ResolveBuffer(mesh.vertexBuffer);
        if (!vb)
        {
            return;
        }
        m_Impl->runtime->NativeResources().MarkBufferUsed(
            mesh.vertexBuffer,
            activeFrameSerial);
        if (vb != lastVB)
        {
            Diligent::Uint64 offsets[] = {0};
            ctx->SetVertexBuffers(0, 1, &vb, offsets,
                                  Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                  Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
            lastVB = vb;
        }

        if (mesh.indexBuffer.IsValid() && mesh.indexCount > 0)
        {
            auto* ib = m_Impl->runtime->NativeResources().ResolveBuffer(mesh.indexBuffer);
            if (!ib)
            {
                return;
            }
            m_Impl->runtime->NativeResources().MarkBufferUsed(
                mesh.indexBuffer,
                activeFrameSerial);
            if (ib != lastIB)
            {
                ctx->SetIndexBuffer(ib, 0,
                                    Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                lastIB = ib;
            }
        }
    };

    if (shadowRequested && ensureShadowResources())
    {
        using namespace Diligent;

        ctx->SetRenderTargets(
            0, nullptr, m_Impl->shadow.dsv,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        Viewport shadowVp{
            0.0f, 0.0f,
            static_cast<float>(m_Impl->shadow.width),
            static_cast<float>(m_Impl->shadow.height),
            0.0f, 1.0f};
        ctx->SetViewports(1, &shadowVp, m_Impl->shadow.width, m_Impl->shadow.height);
        Rect shadowScissor{
            0, 0,
            static_cast<Int32>(m_Impl->shadow.width),
            static_cast<Int32>(m_Impl->shadow.height)};
        ctx->SetScissorRects(1, &shadowScissor, m_Impl->shadow.width, m_Impl->shadow.height);
        ctx->ClearDepthStencil(
            m_Impl->shadow.dsv,
            CLEAR_DEPTH_FLAG,
            1.0f, 0,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->SetPipelineState(m_Impl->shadow.depthPSO);

        IBuffer* lastShadowVB = nullptr;
        IBuffer* lastShadowIB = nullptr;
        for (const auto& item : view.drawItems)
        {
            auto meshIt = m_Impl->meshCache.find(item.mesh);
            auto matIt = m_Impl->materialCache.find(item.material);
            if (meshIt == m_Impl->meshCache.end() ||
                matIt == m_Impl->materialCache.end() ||
                !matIt->second.castsShadows)
            {
                continue;
            }

            NormalMatrixRows normalRows{};
            uploadCameraCB(item.model, normalRows, 0.0f, false);
            bindMeshBuffers(meshIt->second, lastShadowVB, lastShadowIB);

            if (meshIt->second.indexBuffer.IsValid() &&
                meshIt->second.indexCount > 0)
            {
                DrawIndexedAttribs drawAttribs;
                drawAttribs.NumIndices = meshIt->second.indexCount;
                drawAttribs.IndexType  = VT_UINT32;
                drawAttribs.Flags      = m_Impl->gpuValidation ? DRAW_FLAG_VERIFY_ALL
                                                      : DRAW_FLAG_NONE;
                ctx->DrawIndexed(drawAttribs);
                ++m_Impl->currentFrameDiagnostics.shadowPassDrawCalls;
            }
        }

        ++m_Impl->currentFrameDiagnostics.shadowPassExecuted;

        auto* rtv = m_Impl->runtime->SwapChain()->GetCurrentBackBufferRTV();
        auto* dsv = m_Impl->runtime->SwapChain()->GetDepthBufferDSV();
        ctx->SetRenderTargets(
            rtv ? 1u : 0u, rtv ? &rtv : nullptr, dsv,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->SetViewports(1, nullptr, 0, 0);
        ctx->SetScissorRects(0, nullptr, 0, 0);
    }

    // ── Draw loop ────────────────────────────────────────────────────
    Diligent::IPipelineState* lastPSO = nullptr;
    Diligent::IBuffer*        lastVB  = nullptr;
    Diligent::IBuffer*        lastIB  = nullptr;

    int drawIdx = 0;
    for (const auto& item : view.drawItems)
    {
        auto meshIt = m_Impl->meshCache.find(item.mesh);
        if (meshIt == m_Impl->meshCache.end())
        {
            LOG_CAT_DEBUG(Render, "Submit: mesh not in cache, skip");
            ++m_Impl->currentFrameDiagnostics.rejectedDrawItems;
            continue;
        }

        auto matIt = m_Impl->materialCache.find(item.material);
        if (matIt == m_Impl->materialCache.end())
        {
            LOG_CAT_DEBUG(Render, "Submit: material not in cache, skip");
            ++m_Impl->currentFrameDiagnostics.rejectedDrawItems;
            continue;
        }

        auto& mesh = meshIt->second;
        auto& mat  = matIt->second;

        NormalMatrixRows normalRows{};
        if (mat.shadingModel == OpaqueShadingModel::LitDiffuse &&
            !ComputeNormalMatrixRows(item.model, normalRows))
        {
            LOG_CAT_DEBUG(Render, "Submit: singular normal transform, skip lit draw");
            ++m_Impl->currentFrameDiagnostics.rejectedDrawItems;
            ++m_Impl->currentFrameDiagnostics.invalidNormalTransformDraws;
            continue;
        }

        ++m_Impl->currentFrameDiagnostics.submittedDrawItems;

        if (m_Impl->gpuValidation)
        {
            auto* dbgVB = m_Impl->runtime->NativeResources().ResolveBuffer(mesh.vertexBuffer);
            auto* dbgIB = m_Impl->runtime->NativeResources().ResolveBuffer(mesh.indexBuffer);
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "Submit[%d]: verts=%u idx=%u stride=%u pso=%p binding=%u:%u vb=%p ib=%p",
                drawIdx, mesh.vertexCount, mesh.indexCount, mesh.vertexStride,
                static_cast<void*>(mat.pso.RawPtr()),
                mat.binding.index,
                mat.binding.generation,
                static_cast<void*>(dbgVB),
                static_cast<void*>(dbgIB));
            LOG_CAT_DEBUG(Render, buf);
        }

        const bool sampleShadows =
            shadowRequested &&
            mat.shadingModel == OpaqueShadingModel::LitDiffuse &&
            mat.receivesShadows &&
            m_Impl->shadow.srv;
        uploadCameraCB(
            item.model,
            normalRows,
            mat.shadingModel == OpaqueShadingModel::LitDiffuse ? 1.0f : 0.0f,
            sampleShadows);
        if (sampleShadows)
            ++m_Impl->currentFrameDiagnostics.shadowSamplingEnabled;
        LOG_CAT_DEBUG(Render, "Submit: CameraCB mapped OK");

        // ── Skinned draw? Upload bone palette to BoneCB. ────────────
        const bool isSkinned = (item.boneMatrices != nullptr && item.boneCount > 0);
        if (isSkinned)
        {
            Diligent::MapHelper<float> boneData(ctx, m_Impl->boneCB,
                                                 Diligent::MAP_WRITE,
                                                 Diligent::MAP_FLAG_DISCARD);
            // Copy bone matrices (each Mat4 = 16 floats = 64 bytes).
            const std::size_t floatCount = static_cast<std::size_t>(item.boneCount) * 16;
            std::memcpy(&boneData[0], item.boneMatrices, floatCount * sizeof(float));
            // Zero remaining slots so the shader reads identity-ish data
            // if any index overshoots (defensive).
            if (item.boneCount < 128)
            {
                const std::size_t remaining = (128 - item.boneCount) * 16;
                std::memset(&boneData[floatCount], 0, remaining * sizeof(float));
            }
            LOG_CAT_DEBUG(Render, "Submit: BoneCB uploaded");

            // Lazily create skinned PSO + SRB for this material if needed.
            if (!mat.skinnedPso)
            {
                PSOCacheKey key{};
                key.cullMode    = static_cast<std::uint8_t>(mat.cullMode);
                key.renderQueue = static_cast<std::uint8_t>(mat.renderQueue);
                key.writesDepth = mat.writesDepth;

                mat.skinnedPso = FindOrCreateSkinnedPSO(
                    m_Impl->skinnedPsoCache, *m_Impl->runtime->Device(), *m_Impl->runtime->SwapChain(),
                    m_Impl->skinnedVS, m_Impl->solidPS,
                    m_Impl->cameraCB, m_Impl->boneCB, key);

                if (mat.skinnedPso)
                {
                    mat.skinnedBinding =
                        m_Impl->runtime->CreateMaterialBinding(
                            *mat.skinnedPso);
                }
            }
        }

        // Choose PSO and SRB based on skinning.
        Diligent::IPipelineState*         activePso = nullptr;
        auto activeBinding =
            DiligentRuntime::NativeShaderBindingView{};

        if (isSkinned && mat.skinnedPso && mat.skinnedBinding.IsValid())
        {
            activePso = mat.skinnedPso.RawPtr();
            activeBinding =
                m_Impl->runtime->ResolveMaterialBinding(mat.skinnedBinding);
        }
        else
        {
            activePso = mat.pso.RawPtr();
            activeBinding =
                m_Impl->runtime->ResolveMaterialBinding(mat.binding);
        }

        if (!activePso || !activeBinding.IsValid())
        {
            ++m_Impl->currentFrameDiagnostics.rejectedDrawItems;
            continue;
        }

        // Bind albedo texture SRV on the active SRB.
        {
            Diligent::ITextureView* texSRV = nullptr;
            if (mat.albedoTex != TextureHandle::kInvalid)
            {
                auto texIt = m_Impl->textureCache.find(mat.albedoTex);
                if (texIt != m_Impl->textureCache.end())
                {
                    texSRV = m_Impl->runtime->NativeResources().ResolveTextureSrv(
                        texIt->second.texture);
                    if (texSRV)
                    {
                        m_Impl->runtime->NativeResources().MarkTextureUsed(
                            texIt->second.texture,
                            activeFrameSerial);
                    }
                }
            }
            if (!texSRV)
            {
                Gfx::Backends::Diligent::DiligentNativeBindingDiagnostics
                    fallbackDiagnostics{};
                const auto fallback =
                    m_Impl->runtime->FallbackResources().ResolveWhiteTexture(
                        m_Impl->runtime->NativeResources(),
                        fallbackDiagnostics);
                texSRV = fallback.textureView;
                if (fallback.nativeToken.IsValid())
                {
                    m_Impl->runtime->NativeResources().MarkTextureUsed(
                        fallback.nativeToken,
                        activeFrameSerial);
                }
            }

            if (texSRV && activeBinding.albedoVariable)
            {
                activeBinding.albedoVariable->Set(texSRV);
            }

            if (activeBinding.shadowVariable)
            {
                if (m_Impl->shadow.srv)
                    activeBinding.shadowVariable->Set(m_Impl->shadow.srv);
                else if (texSRV)
                    activeBinding.shadowVariable->Set(texSRV);
            }
        }

        // PSO switch
        if (activePso != lastPSO)
        {
            ctx->SetPipelineState(activePso);
            lastPSO = activePso;
            LOG_CAT_DEBUG(Render, "Submit: SetPipelineState OK");
        }

        // SRB
        ctx->CommitShaderResources(activeBinding.srb,
                                   Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        LOG_CAT_DEBUG(Render, "Submit: CommitShaderResources OK");

        // VB
        auto* vb = m_Impl->runtime->NativeResources().ResolveBuffer(mesh.vertexBuffer);
        if (!vb)
        {
            ++m_Impl->currentFrameDiagnostics.rejectedDrawItems;
            continue;
        }
        m_Impl->runtime->NativeResources().MarkBufferUsed(
            mesh.vertexBuffer,
            activeFrameSerial);
        if (vb != lastVB)
        {
            Diligent::Uint64 offsets[] = {0};
            ctx->SetVertexBuffers(0, 1, &vb, offsets,
                                  Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                  Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
            lastVB = vb;
            LOG_CAT_DEBUG(Render, "Submit: SetVertexBuffers OK");
        }

        // IB + Draw
        if (mesh.indexBuffer.IsValid() && mesh.indexCount > 0)
        {
            auto* ib = m_Impl->runtime->NativeResources().ResolveBuffer(mesh.indexBuffer);
            if (!ib)
            {
                ++m_Impl->currentFrameDiagnostics.rejectedDrawItems;
                continue;
            }
            m_Impl->runtime->NativeResources().MarkBufferUsed(
                mesh.indexBuffer,
                activeFrameSerial);
            if (ib != lastIB)
            {
                ctx->SetIndexBuffer(ib, 0,
                                    Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                lastIB = ib;
                LOG_CAT_DEBUG(Render, "Submit: SetIndexBuffer OK");
            }

            Diligent::DrawIndexedAttribs drawAttribs;
            drawAttribs.NumIndices = mesh.indexCount;
            drawAttribs.IndexType  = Diligent::VT_UINT32;
            drawAttribs.Flags      = m_Impl->gpuValidation ? Diligent::DRAW_FLAG_VERIFY_ALL
                                                  : Diligent::DRAW_FLAG_NONE;
            ctx->DrawIndexed(drawAttribs);
            ++m_Impl->currentFrameDiagnostics.indexedDrawCalls;
            ++m_Impl->currentFrameDiagnostics.mainPassDrawCalls;
            m_Impl->currentFrameDiagnostics.lastIndexedIndexCount = mesh.indexCount;
            LOG_CAT_DEBUG(Render, "Submit: DrawIndexed OK");
        }
        else
        {
            Diligent::DrawAttribs drawAttribs;
            drawAttribs.NumVertices = mesh.vertexCount;
            drawAttribs.Flags       = m_Impl->gpuValidation ? Diligent::DRAW_FLAG_VERIFY_ALL
                                                   : Diligent::DRAW_FLAG_NONE;
            ctx->Draw(drawAttribs);
            ++m_Impl->currentFrameDiagnostics.nonIndexedDrawCalls;
            ++m_Impl->currentFrameDiagnostics.mainPassDrawCalls;
            LOG_CAT_DEBUG(Render, "Submit: Draw OK");
        }

        ++drawIdx;
    }
    LOG_CAT_DEBUG(Render, "Submit: done");
}


} // namespace SagaEngine::Render::Backend
