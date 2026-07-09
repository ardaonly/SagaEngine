/// @file DiligentNativeResourceOwner.cpp
/// @brief Private Saga-owned Diligent resource factory and quarantine owner.

#include "SagaEngine/Render/Backend/Diligent/DiligentNativeResourceOwner.h"

#include "RefCntAutoPtr.hpp"
#include "RenderDevice.h"
#include "Buffer.h"
#include "Texture.h"
#include "Sampler.h"
#include "Shader.h"
#include "PipelineState.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

namespace SagaEngine::Render::Backend
{

namespace Gfx = ::SagaEngine::Graphics;

namespace
{

template <typename HandleT>
[[nodiscard]] constexpr HandleT MakeHandle(
    std::uint32_t index,
    std::uint32_t generation) noexcept
{
    HandleT handle{};
    handle.index = index;
    handle.generation = generation;
    return handle;
}

template <typename HandleT>
[[nodiscard]] bool SameHandle(HandleT lhs, HandleT rhs) noexcept
{
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
}

void SetDiagnostic(std::string* out, const char* message)
{
    if (out)
    {
        *out = message;
    }
}

constexpr std::uint64_t kMaxB3NativeBufferBytes =
    256ull * 1024ull * 1024ull;
constexpr std::uint64_t kMaxM4NativeTextureBytes =
    256ull * 1024ull * 1024ull;

[[nodiscard]] Diligent::BIND_FLAGS ToBindFlags(Gfx::BufferUsage usage) noexcept
{
    using namespace Diligent;
    switch (usage)
    {
    case Gfx::BufferUsage::Vertex:
        return BIND_VERTEX_BUFFER;
    case Gfx::BufferUsage::Index:
        return BIND_INDEX_BUFFER;
    case Gfx::BufferUsage::Uniform:
        return BIND_UNIFORM_BUFFER;
    case Gfx::BufferUsage::Storage:
    default:
        return BIND_NONE;
    }
}

[[nodiscard]] Diligent::TEXTURE_FORMAT ToTextureFormat(
    Gfx::ResourceFormat format) noexcept
{
    using namespace Diligent;
    switch (format)
    {
    case Gfx::ResourceFormat::Rgba8Unorm:
        return TEX_FORMAT_RGBA8_UNORM;
    case Gfx::ResourceFormat::Rgba8UnormSrgb:
        return TEX_FORMAT_RGBA8_UNORM_SRGB;
    case Gfx::ResourceFormat::Bgra8Unorm:
        return TEX_FORMAT_BGRA8_UNORM;
    case Gfx::ResourceFormat::Depth24Stencil8:
        return TEX_FORMAT_D24_UNORM_S8_UINT;
    case Gfx::ResourceFormat::Depth32Float:
        return TEX_FORMAT_D32_FLOAT;
    case Gfx::ResourceFormat::Rgba16Float:
        return TEX_FORMAT_RGBA16_FLOAT;
    case Gfx::ResourceFormat::Rgba32Float:
        return TEX_FORMAT_RGBA32_FLOAT;
    case Gfx::ResourceFormat::Unknown:
    default:
        return TEX_FORMAT_UNKNOWN;
    }
}

[[nodiscard]] constexpr std::uint64_t BytesPerPixel(
    Gfx::ResourceFormat format) noexcept
{
    switch (format)
    {
    case Gfx::ResourceFormat::Rgba8Unorm:
    case Gfx::ResourceFormat::Rgba8UnormSrgb:
    case Gfx::ResourceFormat::Bgra8Unorm:
    case Gfx::ResourceFormat::Depth24Stencil8:
    case Gfx::ResourceFormat::Depth32Float:
        return 4u;
    case Gfx::ResourceFormat::Rgba16Float:
        return 8u;
    case Gfx::ResourceFormat::Rgba32Float:
        return 16u;
    case Gfx::ResourceFormat::Unknown:
    default:
        return 0u;
    }
}

[[nodiscard]] Diligent::FILTER_TYPE ToFilter(Gfx::FilterMode mode) noexcept
{
    return mode == Gfx::FilterMode::Nearest ? Diligent::FILTER_TYPE_POINT
                                            : Diligent::FILTER_TYPE_LINEAR;
}

[[nodiscard]] Diligent::TEXTURE_ADDRESS_MODE ToAddress(
    Gfx::AddressMode mode) noexcept
{
    using namespace Diligent;
    switch (mode)
    {
    case Gfx::AddressMode::ClampToEdge:
        return TEXTURE_ADDRESS_CLAMP;
    case Gfx::AddressMode::Repeat:
        return TEXTURE_ADDRESS_WRAP;
    case Gfx::AddressMode::MirrorRepeat:
        return TEXTURE_ADDRESS_MIRROR;
    default:
        return TEXTURE_ADDRESS_UNKNOWN;
    }
}

[[nodiscard]] Diligent::SHADER_TYPE ToShaderType(
    Gfx::ShaderStage stage) noexcept
{
    switch (stage)
    {
    case Gfx::ShaderStage::Vertex:
        return Diligent::SHADER_TYPE_VERTEX;
    case Gfx::ShaderStage::Fragment:
        return Diligent::SHADER_TYPE_PIXEL;
    default:
        return Diligent::SHADER_TYPE_UNKNOWN;
    }
}

[[nodiscard]] Diligent::PRIMITIVE_TOPOLOGY ToTopology(
    Gfx::PrimitiveTopology topology) noexcept
{
    using namespace Diligent;
    switch (topology)
    {
    case Gfx::PrimitiveTopology::TriangleList:
        return PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case Gfx::PrimitiveTopology::TriangleStrip:
        return PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case Gfx::PrimitiveTopology::LineList:
        return PRIMITIVE_TOPOLOGY_LINE_LIST;
    default:
        return PRIMITIVE_TOPOLOGY_UNDEFINED;
    }
}

[[nodiscard]] Diligent::LayoutElement ToLayoutElement(
    const Gfx::VertexAttributeDesc& attr) noexcept
{
    using namespace Diligent;
    switch (attr.format)
    {
    case Gfx::VertexElementFormat::Float32x2:
        return {attr.semanticIndex, attr.bufferSlot, 2u, VT_FLOAT32, False,
                attr.offsetBytes};
    case Gfx::VertexElementFormat::Float32x3:
        return {attr.semanticIndex, attr.bufferSlot, 3u, VT_FLOAT32, False,
                attr.offsetBytes};
    case Gfx::VertexElementFormat::Float32x4:
        return {attr.semanticIndex, attr.bufferSlot, 4u, VT_FLOAT32, False,
                attr.offsetBytes};
    case Gfx::VertexElementFormat::Uint8x4Norm:
        return {attr.semanticIndex, attr.bufferSlot, 4u, VT_UINT8, True,
                attr.offsetBytes};
    case Gfx::VertexElementFormat::Uint8x4:
    default:
        return {attr.semanticIndex, attr.bufferSlot, 4u, VT_UINT8, False,
                attr.offsetBytes};
    }
}

[[nodiscard]] std::vector<std::uint8_t> CopyBufferPayload(
    const Gfx::BufferDesc& desc,
    Gfx::GraphicsDataView initialData)
{
    std::vector<std::uint8_t> payload(
        static_cast<std::size_t>(desc.sizeBytes),
        0u);
    if (initialData.data && initialData.sizeBytes != 0u)
    {
        std::memcpy(
            payload.data(),
            initialData.data,
            static_cast<std::size_t>(initialData.sizeBytes));
    }
    return payload;
}

[[nodiscard]] std::vector<std::uint8_t> CopyTexturePayload(
    const Gfx::TextureDesc& desc,
    Gfx::GraphicsDataView initialData)
{
    const auto bpp = BytesPerPixel(desc.format);
    const auto rowBytes = static_cast<std::uint64_t>(desc.width) * bpp;
    std::vector<std::uint8_t> payload(
        static_cast<std::size_t>(rowBytes * desc.height),
        0u);

    if (!initialData.data || initialData.sizeBytes == 0u)
    {
        return payload;
    }

    const auto srcRowPitch = initialData.rowPitchBytes == 0u
                                 ? rowBytes
                                 : initialData.rowPitchBytes;
    const auto* src = static_cast<const std::uint8_t*>(initialData.data);
    for (std::uint32_t y = 0; y < desc.height; ++y)
    {
        std::memcpy(
            payload.data() + static_cast<std::size_t>(y * rowBytes),
            src + static_cast<std::size_t>(y * srcRowPitch),
            static_cast<std::size_t>(rowBytes));
    }
    return payload;
}

[[nodiscard]] bool IsSupportedNativeTextureDesc(
    const Gfx::TextureDesc& desc) noexcept
{
    if (desc.dimension != Gfx::TextureDimension::Texture2D ||
        desc.width == 0u || desc.height == 0u || desc.depth != 1u ||
        desc.mipLevels != 1u || desc.arrayLayers != 1u ||
        desc.usage != Gfx::TextureUsageFlags::Sampled)
    {
        return false;
    }

    if (desc.format != Gfx::ResourceFormat::Rgba8Unorm &&
        desc.format != Gfx::ResourceFormat::Rgba8UnormSrgb)
    {
        return false;
    }

    const auto bpp = BytesPerPixel(desc.format);
    if (bpp == 0u ||
        desc.width > std::numeric_limits<std::uint64_t>::max() / bpp)
    {
        return false;
    }

    const auto rowBytes = static_cast<std::uint64_t>(desc.width) * bpp;
    if (desc.height >
        std::numeric_limits<std::uint64_t>::max() / rowBytes)
    {
        return false;
    }

    return rowBytes * static_cast<std::uint64_t>(desc.height) <=
           kMaxM4NativeTextureBytes;
}

[[nodiscard]] bool IsSupportedNativeTextureData(
    const Gfx::TextureDesc& desc,
    Gfx::GraphicsDataView initialData) noexcept
{
    if (!initialData.data || initialData.sizeBytes == 0u)
    {
        return false;
    }

    const auto bpp = BytesPerPixel(desc.format);
    const auto rowBytes = static_cast<std::uint64_t>(desc.width) * bpp;
    if (initialData.rowPitchBytes != 0u &&
        static_cast<std::uint64_t>(initialData.rowPitchBytes) < rowBytes)
    {
        return false;
    }

    const auto srcRowPitch =
        initialData.rowPitchBytes == 0u
            ? rowBytes
            : static_cast<std::uint64_t>(initialData.rowPitchBytes);
    if (desc.height >
        std::numeric_limits<std::uint64_t>::max() / srcRowPitch)
    {
        return false;
    }

    const auto requiredBytes =
        srcRowPitch * static_cast<std::uint64_t>(desc.height);
    return initialData.sizeBytes >= requiredBytes;
}

} // namespace

struct DiligentNativeResourceOwner::Impl
{
    template <typename HandleT, typename PayloadT>
    struct Slot
    {
        DiligentNativeResourceToken token{};
        HandleT handle{};
        std::uint64_t serial = 0;
        std::uint64_t creationSerial = 0;
        std::uint64_t lastUseSerial = 0;
        std::uint64_t retireSerial = 0;
        std::uint64_t logicalBytes = 0;
        std::string debugName;
        bool live = false;
        bool pendingDestroy = false;
        PayloadT payload{};
    };

    struct BufferPayload
    {
        Diligent::RefCntAutoPtr<Diligent::IBuffer> buffer;
    };

    struct TexturePayload
    {
        Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
        Diligent::RefCntAutoPtr<Diligent::ITextureView> srv;
        Gfx::TextureDesc desc{};
    };

    struct SamplerPayload
    {
        Diligent::RefCntAutoPtr<Diligent::ISampler> sampler;
        Gfx::SamplerDesc desc{};
    };

    struct ShaderPayload
    {
        Diligent::RefCntAutoPtr<Diligent::IShader> shader;
        Gfx::ShaderDesc desc{};
    };

    struct PipelinePayload
    {
        Diligent::RefCntAutoPtr<Diligent::IPipelineState> pipeline;
        Gfx::PipelineDesc desc{};
    };

    DiligentDeviceServices services{};
    std::uint64_t nextSerial = 1;
    std::uint32_t nextNativeIndex = 1;
    std::uint32_t nextBufferIndex = 1;
    std::uint32_t nextTextureIndex = 1;

    std::vector<Slot<Gfx::BufferHandle, BufferPayload>> buffers;
    std::vector<Slot<Gfx::TextureHandle, TexturePayload>> textures;
    std::vector<Slot<Gfx::SamplerHandle, SamplerPayload>> samplers;
    std::vector<Slot<Gfx::ShaderHandle, ShaderPayload>> shaders;
    std::vector<Slot<Gfx::PipelineHandle, PipelinePayload>> pipelines;

    template <typename SlotT, typename HandleT>
    [[nodiscard]] SlotT* Find(
        std::vector<SlotT>& slots,
        HandleT handle) noexcept
    {
        for (auto& slot : slots)
        {
            if (slot.live && SameHandle(slot.handle, handle))
            {
                return &slot;
            }
        }
        return nullptr;
    }

    template <typename SlotT>
    [[nodiscard]] SlotT* Find(
        std::vector<SlotT>& slots,
        DiligentNativeResourceToken token) noexcept
    {
        if (!token.IsValid())
        {
            return nullptr;
        }
        for (auto& slot : slots)
        {
            if (slot.live && slot.token == token)
            {
                return &slot;
            }
        }
        return nullptr;
    }

    template <typename SlotT, typename HandleT>
    [[nodiscard]] SlotT* FindAny(
        std::vector<SlotT>& slots,
        HandleT handle) noexcept
    {
        for (auto& slot : slots)
        {
            if ((slot.live || slot.pendingDestroy) &&
                SameHandle(slot.handle, handle))
            {
                return &slot;
            }
        }
        return nullptr;
    }

    template <typename SlotT>
    [[nodiscard]] SlotT* FindAny(
        std::vector<SlotT>& slots,
        DiligentNativeResourceToken token) noexcept
    {
        if (!token.IsValid())
        {
            return nullptr;
        }
        for (auto& slot : slots)
        {
            if ((slot.live || slot.pendingDestroy) && slot.token == token)
            {
                return &slot;
            }
        }
        return nullptr;
    }

    template <typename SlotT, typename HandleT>
    [[nodiscard]] const SlotT* Find(
        const std::vector<SlotT>& slots,
        HandleT handle) const noexcept
    {
        for (const auto& slot : slots)
        {
            if (slot.live && SameHandle(slot.handle, handle))
            {
                return &slot;
            }
        }
        return nullptr;
    }

    template <typename SlotT>
    [[nodiscard]] const SlotT* Find(
        const std::vector<SlotT>& slots,
        DiligentNativeResourceToken token) const noexcept
    {
        if (!token.IsValid())
        {
            return nullptr;
        }
        for (const auto& slot : slots)
        {
            if (slot.live && slot.token == token)
            {
                return &slot;
            }
        }
        return nullptr;
    }

    template <typename SlotT, typename HandleT>
    void MarkUsed(
        std::vector<SlotT>& slots,
        HandleT handle,
        std::uint64_t serial) noexcept
    {
        if (serial == 0u)
        {
            return;
        }

        if (auto* slot = FindAny(slots, handle))
        {
            slot->lastUseSerial = std::max(slot->lastUseSerial, serial);
            if (slot->pendingDestroy)
            {
                slot->retireSerial = slot->lastUseSerial;
            }
        }
    }

    [[nodiscard]] DiligentNativeResourceToken AllocateToken(
        DiligentNativeResourceKind kind) noexcept
    {
        if (kind == DiligentNativeResourceKind::Invalid)
        {
            return {};
        }
        return {kind, nextNativeIndex++, 1u};
    }

    template <typename SlotT>
    [[nodiscard]] bool ContainsToken(
        const std::vector<SlotT>& slots,
        DiligentNativeResourceToken token) const noexcept
    {
        if (!token.IsValid())
        {
            return false;
        }
        for (const auto& slot : slots)
        {
            if ((slot.live || slot.pendingDestroy) && slot.token == token)
            {
                return true;
            }
        }
        return false;
    }

    template <typename SlotT>
    static void RetireSlots(
        std::vector<SlotT>& slots,
        std::uint64_t completedSerial) noexcept
    {
        slots.erase(
            std::remove_if(
                slots.begin(),
                slots.end(),
                [completedSerial](const SlotT& slot)
                {
                    return slot.pendingDestroy &&
                           slot.retireSerial <= completedSerial;
                }),
            slots.end());
    }
};

DiligentNativeResourceOwner::DiligentNativeResourceOwner()
    : m_Impl(std::make_unique<Impl>())
{
}

DiligentNativeResourceOwner::~DiligentNativeResourceOwner() = default;

void DiligentNativeResourceOwner::Bind(
    DiligentDeviceServices services) noexcept
{
    m_Impl->services = services;
}

void DiligentNativeResourceOwner::ReleaseAll() noexcept
{
    m_Impl->pipelines.clear();
    m_Impl->shaders.clear();
    m_Impl->samplers.clear();
    m_Impl->textures.clear();
    m_Impl->buffers.clear();
    m_Impl->services = {};
}

bool DiligentNativeResourceOwner::CanCreateNative() const noexcept
{
    return m_Impl->services.IsBound();
}

bool DiligentNativeResourceOwner::IsBufferUsageSupported(
    Gfx::BufferUsage usage) noexcept
{
    return ToBindFlags(usage) != Diligent::BIND_NONE;
}

bool DiligentNativeResourceOwner::IsBufferSizeSupported(
    std::uint64_t sizeBytes) noexcept
{
    return sizeBytes != 0u &&
           sizeBytes <=
               static_cast<std::uint64_t>(
                   std::numeric_limits<std::size_t>::max()) &&
           sizeBytes <= kMaxB3NativeBufferBytes;
}

DiligentNativeResourceToken DiligentNativeResourceOwner::AllocateToken(
    DiligentNativeResourceKind kind) noexcept
{
    return m_Impl->AllocateToken(kind);
}

std::uint64_t DiligentNativeResourceOwner::CreateBufferForToken(
    DiligentNativeBufferToken token,
    const Gfx::BufferDesc& desc,
    Gfx::GraphicsDataView initialData,
    std::string* diagnostic)
{
    if (token.kind != DiligentNativeResourceKind::Buffer ||
        m_Impl->ContainsToken(m_Impl->buffers, token))
    {
        SetDiagnostic(diagnostic, "Duplicate or wrong-kind native buffer token");
        return 0u;
    }

    if (!CanCreateNative())
    {
        SetDiagnostic(diagnostic, "Diligent device services are not bound");
        return 0u;
    }

    if (!IsBufferSizeSupported(desc.sizeBytes))
    {
        SetDiagnostic(diagnostic, "Unsupported Saga buffer size");
        return 0u;
    }

    if (initialData.sizeBytes != 0u &&
        (!initialData.data || initialData.sizeBytes > desc.sizeBytes))
    {
        SetDiagnostic(diagnostic, "Invalid Saga buffer initial data");
        return 0u;
    }

    const auto bindFlags = ToBindFlags(desc.usage);
    if (bindFlags == Diligent::BIND_NONE)
    {
        SetDiagnostic(diagnostic, "Unsupported Saga buffer usage");
        return 0u;
    }

    auto payload = CopyBufferPayload(desc, initialData);

    Diligent::BufferDesc nativeDesc;
    nativeDesc.Name = desc.debugName.empty() ? nullptr : desc.debugName.c_str();
    nativeDesc.Size = desc.sizeBytes;
    nativeDesc.BindFlags = bindFlags;
    nativeDesc.Usage = desc.dynamic ? Diligent::USAGE_DYNAMIC
                                    : Diligent::USAGE_IMMUTABLE;
    nativeDesc.CPUAccessFlags =
        desc.dynamic ? Diligent::CPU_ACCESS_WRITE
                     : Diligent::CPU_ACCESS_NONE;

    Diligent::BufferData data;
    data.pData = payload.data();
    data.DataSize = desc.sizeBytes;

    Diligent::RefCntAutoPtr<Diligent::IBuffer> buffer;
    m_Impl->services.Device()->CreateBuffer(nativeDesc, &data, &buffer);
    if (!buffer)
    {
        SetDiagnostic(diagnostic, "Diligent buffer creation failed");
        return 0u;
    }

    const auto serial = m_Impl->nextSerial++;
    m_Impl->buffers.push_back(
        {token, {}, serial, serial, 0u, 0u, desc.sizeBytes, desc.debugName,
         true, false, {buffer}});
    return serial;
}

std::uint64_t DiligentNativeResourceOwner::CreateTextureForToken(
    DiligentNativeTextureToken token,
    const Gfx::TextureDesc& desc,
    Gfx::GraphicsDataView initialData,
    std::string* diagnostic)
{
    if (token.kind != DiligentNativeResourceKind::Texture ||
        m_Impl->ContainsToken(m_Impl->textures, token))
    {
        SetDiagnostic(diagnostic, "Duplicate or wrong-kind native texture token");
        return 0u;
    }

    if (!CanCreateNative())
    {
        SetDiagnostic(diagnostic, "Diligent device services are not bound");
        return 0u;
    }

    if (!IsSupportedNativeTextureDesc(desc))
    {
        SetDiagnostic(diagnostic, "Unsupported native Saga texture descriptor");
        return 0u;
    }

    if (!IsSupportedNativeTextureData(desc, initialData))
    {
        SetDiagnostic(diagnostic, "Invalid native Saga texture initial data");
        return 0u;
    }

    const auto format = ToTextureFormat(desc.format);
    auto payload = CopyTexturePayload(desc, initialData);
    const auto stride = static_cast<Diligent::Uint64>(desc.width) * 4u;

    Diligent::TextureDesc nativeDesc;
    nativeDesc.Name = desc.debugName.empty() ? nullptr : desc.debugName.c_str();
    nativeDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    nativeDesc.Width = desc.width;
    nativeDesc.Height = desc.height;
    nativeDesc.Format = format;
    nativeDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    nativeDesc.Usage = Diligent::USAGE_IMMUTABLE;
    nativeDesc.MipLevels = 1;

    Diligent::TextureSubResData subRes;
    subRes.pData = payload.data();
    subRes.Stride = stride;

    Diligent::TextureData data;
    data.pSubResources = &subRes;
    data.NumSubresources = 1;

    Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
    m_Impl->services.Device()->CreateTexture(nativeDesc, &data, &texture);
    if (!texture)
    {
        SetDiagnostic(diagnostic, "Diligent texture creation failed");
        return 0u;
    }

    Diligent::RefCntAutoPtr<Diligent::ITextureView> srv;
    srv = texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    if (!srv)
    {
        SetDiagnostic(diagnostic, "Diligent texture SRV creation failed");
        return 0u;
    }

    const auto serial = m_Impl->nextSerial++;
    m_Impl->textures.push_back(
        {token, {}, serial, serial, 0u, 0u,
         static_cast<std::uint64_t>(desc.width) * desc.height *
             BytesPerPixel(desc.format),
         desc.debugName, true, false, {texture, srv, desc}});
    return serial;
}

std::uint64_t DiligentNativeResourceOwner::CreateSamplerForToken(
    DiligentNativeSamplerToken token,
    const Gfx::SamplerDesc& desc,
    std::string* diagnostic)
{
    if (token.kind != DiligentNativeResourceKind::Sampler ||
        m_Impl->ContainsToken(m_Impl->samplers, token))
    {
        SetDiagnostic(diagnostic, "Duplicate or wrong-kind native sampler token");
        return 0u;
    }

    if (!CanCreateNative())
    {
        SetDiagnostic(diagnostic, "Diligent device services are not bound");
        return 0u;
    }

    const bool pointClamp =
        desc.minFilter == Gfx::FilterMode::Nearest &&
        desc.magFilter == Gfx::FilterMode::Nearest &&
        desc.addressU == Gfx::AddressMode::ClampToEdge &&
        desc.addressV == Gfx::AddressMode::ClampToEdge;
    const bool linearClamp =
        desc.minFilter == Gfx::FilterMode::Linear &&
        desc.magFilter == Gfx::FilterMode::Linear &&
        desc.addressU == Gfx::AddressMode::ClampToEdge &&
        desc.addressV == Gfx::AddressMode::ClampToEdge;
    const bool linearWrap =
        desc.minFilter == Gfx::FilterMode::Linear &&
        desc.magFilter == Gfx::FilterMode::Linear &&
        desc.addressU == Gfx::AddressMode::Repeat &&
        desc.addressV == Gfx::AddressMode::Repeat;
    if (!pointClamp && !linearClamp && !linearWrap)
    {
        SetDiagnostic(diagnostic, "Unsupported native Saga sampler mode");
        return 0u;
    }

    Diligent::SamplerDesc nativeDesc;
    nativeDesc.Name = desc.debugName.empty() ? nullptr : desc.debugName.c_str();
    nativeDesc.MinFilter = ToFilter(desc.minFilter);
    nativeDesc.MagFilter = ToFilter(desc.magFilter);
    nativeDesc.MipFilter = ToFilter(desc.minFilter);
    nativeDesc.AddressU = ToAddress(desc.addressU);
    nativeDesc.AddressV = ToAddress(desc.addressV);
    nativeDesc.AddressW = ToAddress(desc.addressW);
    nativeDesc.MipLODBias = desc.mipLodBias;

    Diligent::RefCntAutoPtr<Diligent::ISampler> sampler;
    m_Impl->services.Device()->CreateSampler(nativeDesc, &sampler);
    if (!sampler)
    {
        SetDiagnostic(diagnostic, "Diligent sampler creation failed");
        return 0u;
    }

    const auto serial = m_Impl->nextSerial++;
    m_Impl->samplers.push_back(
        {token, {}, serial, serial, 0u, 0u, 0u, desc.debugName,
         true, false, {sampler, desc}});
    return serial;
}

std::uint64_t DiligentNativeResourceOwner::CreateShaderForToken(
    DiligentNativeShaderToken token,
    const Gfx::ShaderDesc& desc,
    std::string* diagnostic)
{
    if (token.kind != DiligentNativeResourceKind::Shader ||
        m_Impl->ContainsToken(m_Impl->shaders, token))
    {
        SetDiagnostic(diagnostic, "Duplicate or wrong-kind native shader token");
        return 0u;
    }

    if (!CanCreateNative())
    {
        SetDiagnostic(diagnostic, "Diligent device services are not bound");
        return 0u;
    }

    const auto shaderType = ToShaderType(desc.stage);
    if (shaderType == Diligent::SHADER_TYPE_UNKNOWN ||
        desc.source.empty() || desc.entryPoint.empty())
    {
        SetDiagnostic(diagnostic, "Native shaders require VS/PS source and entry point");
        return 0u;
    }

    Diligent::ShaderCreateInfo ci;
    ci.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    ci.Desc.UseCombinedTextureSamplers = Diligent::False;
    ci.Desc.ShaderType = shaderType;
    ci.Desc.Name = desc.debugName.empty() ? nullptr : desc.debugName.c_str();
    ci.EntryPoint = desc.entryPoint.c_str();
    ci.Source = desc.source.c_str();

    Diligent::RefCntAutoPtr<Diligent::IShader> shader;
    m_Impl->services.Device()->CreateShader(ci, &shader);
    if (!shader)
    {
        SetDiagnostic(diagnostic, "Diligent shader creation failed");
        return 0u;
    }

    const auto serial = m_Impl->nextSerial++;
    m_Impl->shaders.push_back(
        {token, {}, serial, serial, 0u, 0u,
         static_cast<std::uint64_t>(desc.source.size()), desc.debugName,
         true, false, {shader, desc}});
    return serial;
}

std::uint64_t DiligentNativeResourceOwner::CreatePipelineForToken(
    DiligentNativePipelineToken token,
    const Gfx::PipelineDesc& desc,
    std::string* diagnostic)
{
    const auto* vsSlot = m_Impl->Find(m_Impl->shaders, desc.vertexShader);
    const auto* psSlot = m_Impl->Find(m_Impl->shaders, desc.fragmentShader);
    return CreatePipelineForToken(
        token,
        vsSlot ? vsSlot->token : DiligentNativeShaderToken{},
        psSlot ? psSlot->token : DiligentNativeShaderToken{},
        desc,
        diagnostic);
}

std::uint64_t DiligentNativeResourceOwner::CreatePipelineForToken(
    DiligentNativePipelineToken token,
    DiligentNativeShaderToken vertexShader,
    DiligentNativeShaderToken fragmentShader,
    const Gfx::PipelineDesc& desc,
    std::string* diagnostic)
{
    if (token.kind != DiligentNativeResourceKind::Pipeline ||
        m_Impl->ContainsToken(m_Impl->pipelines, token))
    {
        SetDiagnostic(diagnostic, "Duplicate or wrong-kind native pipeline token");
        return 0u;
    }

    if (!CanCreateNative())
    {
        SetDiagnostic(diagnostic, "Diligent device services are not bound");
        return 0u;
    }

    const auto* vsSlot = m_Impl->Find(m_Impl->shaders, vertexShader);
    const auto* psSlot = m_Impl->Find(m_Impl->shaders, fragmentShader);
    auto* vs = vsSlot ? vsSlot->payload.shader.RawPtr() : nullptr;
    auto* ps = psSlot ? psSlot->payload.shader.RawPtr() : nullptr;
    if (!vs || !ps ||
        vsSlot->payload.desc.stage != Gfx::ShaderStage::Vertex ||
        psSlot->payload.desc.stage != Gfx::ShaderStage::Fragment)
    {
        SetDiagnostic(diagnostic, "Native pipeline requires native VS and PS handles");
        return 0u;
    }

    const auto colorFormat = ToTextureFormat(desc.colorFormat);
    const auto depthFormat = ToTextureFormat(desc.depthFormat);
    if (colorFormat == Diligent::TEX_FORMAT_UNKNOWN ||
        depthFormat == Diligent::TEX_FORMAT_UNKNOWN ||
        desc.sampleCount == 0u)
    {
        SetDiagnostic(diagnostic, "Unsupported native pipeline format/sample state");
        return 0u;
    }

    std::vector<Diligent::LayoutElement> layout;
    layout.reserve(desc.vertexLayout.size());
    for (const auto& attr : desc.vertexLayout)
    {
        layout.push_back(ToLayoutElement(attr));
    }

    Diligent::GraphicsPipelineStateCreateInfo ci;
    ci.PSODesc.Name = desc.debugName.empty() ? nullptr : desc.debugName.c_str();
    ci.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
    ci.PSODesc.ResourceLayout.DefaultVariableType =
        Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
    ci.pVS = vs;
    ci.pPS = ps;
    ci.GraphicsPipeline.NumRenderTargets = desc.colorTargetCount;
    ci.GraphicsPipeline.RTVFormats[0] = colorFormat;
    ci.GraphicsPipeline.DSVFormat = depthFormat;
    ci.GraphicsPipeline.PrimitiveTopology = ToTopology(desc.topology);
    ci.GraphicsPipeline.SmplDesc.Count = desc.sampleCount;
    ci.GraphicsPipeline.DepthStencilDesc.DepthEnable =
        desc.depthTest ? Diligent::True : Diligent::False;
    ci.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable =
        desc.depthWrite ? Diligent::True : Diligent::False;
    ci.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise =
        desc.frontCounterClockwise ? Diligent::True : Diligent::False;
    ci.GraphicsPipeline.RasterizerDesc.CullMode =
        desc.cullBackFaces ? Diligent::CULL_MODE_BACK
                           : Diligent::CULL_MODE_NONE;
    if (!layout.empty())
    {
        ci.GraphicsPipeline.InputLayout.LayoutElements = layout.data();
        ci.GraphicsPipeline.InputLayout.NumElements =
            static_cast<Diligent::Uint32>(layout.size());
    }
    if (desc.alphaBlend)
    {
        auto& rt0 = ci.GraphicsPipeline.BlendDesc.RenderTargets[0];
        rt0.BlendEnable = Diligent::True;
        rt0.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
        rt0.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
        rt0.BlendOp = Diligent::BLEND_OPERATION_ADD;
        rt0.SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
        rt0.DestBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
        rt0.BlendOpAlpha = Diligent::BLEND_OPERATION_ADD;
        rt0.RenderTargetWriteMask = Diligent::COLOR_MASK_ALL;
    }

    Diligent::RefCntAutoPtr<Diligent::IPipelineState> pipeline;
    m_Impl->services.Device()->CreateGraphicsPipelineState(ci, &pipeline);
    if (!pipeline)
    {
        SetDiagnostic(diagnostic, "Diligent graphics pipeline creation failed");
        return 0u;
    }

    const auto serial = m_Impl->nextSerial++;
    m_Impl->pipelines.push_back(
        {token, {}, serial, serial, 0u, 0u, 0u, desc.debugName,
         true, false, {pipeline, desc}});
    return serial;
}

std::uint64_t DiligentNativeResourceOwner::CreateBufferForHandle(
    Gfx::BufferHandle handle,
    const Gfx::BufferDesc& desc,
    Gfx::GraphicsDataView initialData,
    std::string* diagnostic)
{
    const auto token = AllocateToken(DiligentNativeResourceKind::Buffer);
    const auto serial = CreateBufferForToken(
        token,
        desc,
        initialData,
        diagnostic);
    if (serial != 0u)
    {
        if (auto* slot = m_Impl->Find(m_Impl->buffers, token))
        {
            slot->handle = handle;
        }
    }
    return serial;
}

std::uint64_t DiligentNativeResourceOwner::CreateTextureForHandle(
    Gfx::TextureHandle handle,
    const Gfx::TextureDesc& desc,
    Gfx::GraphicsDataView initialData,
    std::string* diagnostic)
{
    const auto token = AllocateToken(DiligentNativeResourceKind::Texture);
    const auto serial = CreateTextureForToken(
        token,
        desc,
        initialData,
        diagnostic);
    if (serial != 0u)
    {
        if (auto* slot = m_Impl->Find(m_Impl->textures, token))
        {
            slot->handle = handle;
        }
    }
    return serial;
}

std::uint64_t DiligentNativeResourceOwner::CreateSamplerForHandle(
    Gfx::SamplerHandle handle,
    const Gfx::SamplerDesc& desc,
    std::string* diagnostic)
{
    const auto token = AllocateToken(DiligentNativeResourceKind::Sampler);
    const auto serial = CreateSamplerForToken(token, desc, diagnostic);
    if (serial != 0u)
    {
        if (auto* slot = m_Impl->Find(m_Impl->samplers, token))
        {
            slot->handle = handle;
        }
    }
    return serial;
}

std::uint64_t DiligentNativeResourceOwner::CreateShaderForHandle(
    Gfx::ShaderHandle handle,
    const Gfx::ShaderDesc& desc,
    std::string* diagnostic)
{
    const auto token = AllocateToken(DiligentNativeResourceKind::Shader);
    const auto serial = CreateShaderForToken(token, desc, diagnostic);
    if (serial != 0u)
    {
        if (auto* slot = m_Impl->Find(m_Impl->shaders, token))
        {
            slot->handle = handle;
        }
    }
    return serial;
}

std::uint64_t DiligentNativeResourceOwner::CreatePipelineForHandle(
    Gfx::PipelineHandle handle,
    const Gfx::PipelineDesc& desc,
    std::string* diagnostic)
{
    const auto token = AllocateToken(DiligentNativeResourceKind::Pipeline);
    const auto serial = CreatePipelineForToken(token, desc, diagnostic);
    if (serial != 0u)
    {
        if (auto* slot = m_Impl->Find(m_Impl->pipelines, token))
        {
            slot->handle = handle;
        }
    }
    return serial;
}

Gfx::BufferHandle DiligentNativeResourceOwner::CreateStandaloneBuffer(
    const Gfx::BufferDesc& desc,
    Gfx::GraphicsDataView initialData,
    std::string* diagnostic)
{
    const auto handle =
        MakeHandle<Gfx::BufferHandle>(m_Impl->nextBufferIndex++, 1u);
    return CreateBufferForHandle(handle, desc, initialData, diagnostic) == 0u
               ? Gfx::BufferHandle{}
               : handle;
}

Gfx::TextureHandle DiligentNativeResourceOwner::CreateStandaloneTexture(
    const Gfx::TextureDesc& desc,
    Gfx::GraphicsDataView initialData,
    std::string* diagnostic)
{
    const auto handle =
        MakeHandle<Gfx::TextureHandle>(m_Impl->nextTextureIndex++, 1u);
    return CreateTextureForHandle(handle, desc, initialData, diagnostic) == 0u
               ? Gfx::TextureHandle{}
               : handle;
}

void DiligentNativeResourceOwner::DestroyBuffer(
    DiligentNativeBufferToken token) noexcept
{
    if (auto* slot = m_Impl->Find(m_Impl->buffers, token))
    {
        slot->live = false;
        slot->pendingDestroy = true;
        slot->retireSerial = slot->lastUseSerial;
    }
}

void DiligentNativeResourceOwner::DestroyTexture(
    DiligentNativeTextureToken token) noexcept
{
    if (auto* slot = m_Impl->Find(m_Impl->textures, token))
    {
        slot->live = false;
        slot->pendingDestroy = true;
        slot->retireSerial = slot->lastUseSerial;
    }
}

void DiligentNativeResourceOwner::DestroySampler(
    DiligentNativeSamplerToken token) noexcept
{
    if (auto* slot = m_Impl->Find(m_Impl->samplers, token))
    {
        slot->live = false;
        slot->pendingDestroy = true;
        slot->retireSerial = slot->lastUseSerial;
    }
}

void DiligentNativeResourceOwner::DestroyShader(
    DiligentNativeShaderToken token) noexcept
{
    if (auto* slot = m_Impl->Find(m_Impl->shaders, token))
    {
        slot->live = false;
        slot->pendingDestroy = true;
        slot->retireSerial = slot->lastUseSerial;
    }
}

void DiligentNativeResourceOwner::DestroyPipeline(
    DiligentNativePipelineToken token) noexcept
{
    if (auto* slot = m_Impl->Find(m_Impl->pipelines, token))
    {
        slot->live = false;
        slot->pendingDestroy = true;
        slot->retireSerial = slot->lastUseSerial;
    }
}

void DiligentNativeResourceOwner::DestroyBuffer(
    Gfx::BufferHandle handle) noexcept
{
    if (auto* slot = m_Impl->Find(m_Impl->buffers, handle))
    {
        slot->live = false;
        slot->pendingDestroy = true;
        slot->retireSerial = slot->lastUseSerial;
    }
}

void DiligentNativeResourceOwner::DestroyTexture(
    Gfx::TextureHandle handle) noexcept
{
    if (auto* slot = m_Impl->Find(m_Impl->textures, handle))
    {
        slot->live = false;
        slot->pendingDestroy = true;
        slot->retireSerial = slot->lastUseSerial;
    }
}

void DiligentNativeResourceOwner::DestroySampler(
    Gfx::SamplerHandle handle) noexcept
{
    if (auto* slot = m_Impl->Find(m_Impl->samplers, handle))
    {
        slot->live = false;
        slot->pendingDestroy = true;
        slot->retireSerial = slot->lastUseSerial;
    }
}

void DiligentNativeResourceOwner::DestroyShader(
    Gfx::ShaderHandle handle) noexcept
{
    if (auto* slot = m_Impl->Find(m_Impl->shaders, handle))
    {
        slot->live = false;
        slot->pendingDestroy = true;
        slot->retireSerial = slot->lastUseSerial;
    }
}

void DiligentNativeResourceOwner::DestroyPipeline(
    Gfx::PipelineHandle handle) noexcept
{
    if (auto* slot = m_Impl->Find(m_Impl->pipelines, handle))
    {
        slot->live = false;
        slot->pendingDestroy = true;
        slot->retireSerial = slot->lastUseSerial;
    }
}

void DiligentNativeResourceOwner::MarkBufferUsed(
    Gfx::BufferHandle handle,
    std::uint64_t serial) noexcept
{
    m_Impl->MarkUsed(m_Impl->buffers, handle, serial);
}

void DiligentNativeResourceOwner::MarkTextureUsed(
    Gfx::TextureHandle handle,
    std::uint64_t serial) noexcept
{
    m_Impl->MarkUsed(m_Impl->textures, handle, serial);
}

void DiligentNativeResourceOwner::MarkSamplerUsed(
    Gfx::SamplerHandle handle,
    std::uint64_t serial) noexcept
{
    m_Impl->MarkUsed(m_Impl->samplers, handle, serial);
}

void DiligentNativeResourceOwner::MarkShaderUsed(
    Gfx::ShaderHandle handle,
    std::uint64_t serial) noexcept
{
    m_Impl->MarkUsed(m_Impl->shaders, handle, serial);
}

void DiligentNativeResourceOwner::MarkPipelineUsed(
    Gfx::PipelineHandle handle,
    std::uint64_t serial) noexcept
{
    m_Impl->MarkUsed(m_Impl->pipelines, handle, serial);
}

void DiligentNativeResourceOwner::MarkBufferUsed(
    DiligentNativeBufferToken token,
    std::uint64_t serial) noexcept
{
    m_Impl->MarkUsed(m_Impl->buffers, token, serial);
}

void DiligentNativeResourceOwner::MarkTextureUsed(
    DiligentNativeTextureToken token,
    std::uint64_t serial) noexcept
{
    m_Impl->MarkUsed(m_Impl->textures, token, serial);
}

void DiligentNativeResourceOwner::MarkSamplerUsed(
    DiligentNativeSamplerToken token,
    std::uint64_t serial) noexcept
{
    m_Impl->MarkUsed(m_Impl->samplers, token, serial);
}

void DiligentNativeResourceOwner::MarkShaderUsed(
    DiligentNativeShaderToken token,
    std::uint64_t serial) noexcept
{
    m_Impl->MarkUsed(m_Impl->shaders, token, serial);
}

void DiligentNativeResourceOwner::MarkPipelineUsed(
    DiligentNativePipelineToken token,
    std::uint64_t serial) noexcept
{
    m_Impl->MarkUsed(m_Impl->pipelines, token, serial);
}

void DiligentNativeResourceOwner::RetireCompleted(
    std::uint64_t completedSerial) noexcept
{
    m_Impl->RetireSlots(m_Impl->pipelines, completedSerial);
    m_Impl->RetireSlots(m_Impl->shaders, completedSerial);
    m_Impl->RetireSlots(m_Impl->samplers, completedSerial);
    m_Impl->RetireSlots(m_Impl->textures, completedSerial);
    m_Impl->RetireSlots(m_Impl->buffers, completedSerial);
}

Diligent::IBuffer* DiligentNativeResourceOwner::ResolveBuffer(
    Gfx::BufferHandle handle) const noexcept
{
    const auto* slot = m_Impl->Find(m_Impl->buffers, handle);
    return slot ? slot->payload.buffer.RawPtr() : nullptr;
}

Diligent::ITexture* DiligentNativeResourceOwner::ResolveTexture(
    Gfx::TextureHandle handle) const noexcept
{
    const auto* slot = m_Impl->Find(m_Impl->textures, handle);
    return slot ? slot->payload.texture.RawPtr() : nullptr;
}

Diligent::ITextureView* DiligentNativeResourceOwner::ResolveTextureSrv(
    Gfx::TextureHandle handle) const noexcept
{
    const auto* slot = m_Impl->Find(m_Impl->textures, handle);
    return slot ? slot->payload.srv.RawPtr() : nullptr;
}

Diligent::ISampler* DiligentNativeResourceOwner::ResolveSampler(
    Gfx::SamplerHandle handle) const noexcept
{
    const auto* slot = m_Impl->Find(m_Impl->samplers, handle);
    return slot ? slot->payload.sampler.RawPtr() : nullptr;
}

Diligent::IShader* DiligentNativeResourceOwner::ResolveShader(
    Gfx::ShaderHandle handle) const noexcept
{
    const auto* slot = m_Impl->Find(m_Impl->shaders, handle);
    return slot ? slot->payload.shader.RawPtr() : nullptr;
}

Diligent::IPipelineState* DiligentNativeResourceOwner::ResolvePipeline(
    Gfx::PipelineHandle handle) const noexcept
{
    const auto* slot = m_Impl->Find(m_Impl->pipelines, handle);
    return slot ? slot->payload.pipeline.RawPtr() : nullptr;
}

Diligent::IBuffer* DiligentNativeResourceOwner::ResolveBuffer(
    DiligentNativeBufferToken token) const noexcept
{
    if (token.kind != DiligentNativeResourceKind::Buffer)
    {
        return nullptr;
    }
    const auto* slot = m_Impl->Find(m_Impl->buffers, token);
    return slot ? slot->payload.buffer.RawPtr() : nullptr;
}

Diligent::ITexture* DiligentNativeResourceOwner::ResolveTexture(
    DiligentNativeTextureToken token) const noexcept
{
    if (token.kind != DiligentNativeResourceKind::Texture)
    {
        return nullptr;
    }
    const auto* slot = m_Impl->Find(m_Impl->textures, token);
    return slot ? slot->payload.texture.RawPtr() : nullptr;
}

Diligent::ITextureView* DiligentNativeResourceOwner::ResolveTextureSrv(
    DiligentNativeTextureToken token) const noexcept
{
    if (token.kind != DiligentNativeResourceKind::Texture)
    {
        return nullptr;
    }
    const auto* slot = m_Impl->Find(m_Impl->textures, token);
    return slot ? slot->payload.srv.RawPtr() : nullptr;
}

Diligent::ISampler* DiligentNativeResourceOwner::ResolveSampler(
    DiligentNativeSamplerToken token) const noexcept
{
    if (token.kind != DiligentNativeResourceKind::Sampler)
    {
        return nullptr;
    }
    const auto* slot = m_Impl->Find(m_Impl->samplers, token);
    return slot ? slot->payload.sampler.RawPtr() : nullptr;
}

Diligent::IShader* DiligentNativeResourceOwner::ResolveShader(
    DiligentNativeShaderToken token) const noexcept
{
    if (token.kind != DiligentNativeResourceKind::Shader)
    {
        return nullptr;
    }
    const auto* slot = m_Impl->Find(m_Impl->shaders, token);
    return slot ? slot->payload.shader.RawPtr() : nullptr;
}

Diligent::IPipelineState* DiligentNativeResourceOwner::ResolvePipeline(
    DiligentNativePipelineToken token) const noexcept
{
    if (token.kind != DiligentNativeResourceKind::Pipeline)
    {
        return nullptr;
    }
    const auto* slot = m_Impl->Find(m_Impl->pipelines, token);
    return slot ? slot->payload.pipeline.RawPtr() : nullptr;
}

} // namespace SagaEngine::Render::Backend
