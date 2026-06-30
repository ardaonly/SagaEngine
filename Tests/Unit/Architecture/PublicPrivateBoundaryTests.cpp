/// @file PublicPrivateBoundaryTests.cpp
/// @brief Guards Public and Private include boundaries for module migration.

#include <gtest/gtest.h>

#include "SagaEngine/Graphics/Graphics.h"
#include "SagaEngine/Render/RenderPipelineConfig.h"
#include "SagaEngine/Render/Scene/RenderFrameSnapshot.h"
#include "SagaEngine/Render/Streaming/RenderStreamingResidency.h"
#include "SagaEngine/Render/World/RenderInterestWorldStreaming.h"
#include "SagaEngine/World/WorldFacade.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

bool IsCodeFile(const std::filesystem::path& path)
{
    const auto ext = path.extension().string();
    return ext == ".h" || ext == ".hh" || ext == ".hpp" || ext == ".hxx" ||
           ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".cxx";
}

std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::vector<std::string> ReadLines(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line))
    {
        lines.push_back(line);
    }
    return lines;
}

std::filesystem::path RelativeToSourceRoot(const std::filesystem::path& path)
{
    return std::filesystem::relative(path, std::filesystem::path(SAGA_SOURCE_ROOT));
}

bool StartsWith(std::string_view value, std::string_view prefix)
{
    return value.size() >= prefix.size() &&
           value.substr(0, prefix.size()) == prefix;
}

bool Contains(std::string_view value, std::string_view token)
{
    return value.find(token) != std::string_view::npos;
}

std::size_t CountOccurrences(std::string_view value, std::string_view token)
{
    std::size_t count = 0;
    std::size_t cursor = 0;
    while ((cursor = value.find(token, cursor)) != std::string_view::npos)
    {
        ++count;
        cursor += token.size();
    }
    return count;
}

std::string_view SliceBetween(
    std::string_view text,
    std::string_view begin,
    std::string_view end)
{
    const auto beginPos = text.find(begin);
    if (beginPos == std::string_view::npos)
    {
        return {};
    }

    const auto endPos = text.find(end, beginPos + begin.size());
    if (endPos == std::string_view::npos)
    {
        return text.substr(beginPos);
    }

    return text.substr(beginPos, endPos - beginPos);
}

} // namespace

TEST(PublicPrivateBoundaryTests, EngineModulesDoNotExposePrivateIncludes)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto engineRoot = root / "Engine";
    ASSERT_TRUE(std::filesystem::exists(engineRoot / "Public"));
    ASSERT_TRUE(std::filesystem::exists(engineRoot / "Private"));
    EXPECT_FALSE(std::filesystem::exists(root / "Source" / "SagaEngine"));
    EXPECT_FALSE(std::filesystem::exists(engineRoot / "include"));
    EXPECT_FALSE(std::filesystem::exists(engineRoot / "src"));

    std::vector<std::filesystem::path> layoutOffenders;
    for (const auto& entry : std::filesystem::directory_iterator(engineRoot))
    {
        if (!entry.is_directory())
        {
            continue;
        }

        const auto name = entry.path().filename().string();
        if (name == "Public" || name == "Private")
        {
            continue;
        }

        if (std::filesystem::exists(entry.path() / "Public") ||
            std::filesystem::exists(entry.path() / "Private"))
        {
            layoutOffenders.push_back(RelativeToSourceRoot(entry.path()));
        }
    }

    EXPECT_TRUE(layoutOffenders.empty())
        << "Only Engine/Public and Engine/Private may own the public/private split. "
        << "First offender: "
        << (layoutOffenders.empty() ? "" : layoutOffenders.front().generic_string());

    std::vector<std::string> offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
        {
            continue;
        }

        const auto relative = RelativeToSourceRoot(entry.path());
        const auto relText = relative.generic_string();
        const auto lines = ReadLines(entry.path());
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            const auto& line = lines[i];
            if (line.find("#include") == std::string::npos ||
                line.find("Private/") == std::string::npos)
            {
                continue;
            }

            offenders.push_back(
                relText + ":" + std::to_string(i + 1) + ": " + line);
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Code must not include paths containing Private/. First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(PublicPrivateBoundaryTests, DiagnosticsPublicHeadersDoNotDependUpward)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto diagnosticsPublic =
        root / "Engine" / "Public" / "SagaEngine" / "Diagnostics";
    ASSERT_TRUE(std::filesystem::exists(diagnosticsPublic));

    std::vector<std::filesystem::path> offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(diagnosticsPublic))
    {
        if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
        {
            continue;
        }

        const auto text = ReadText(entry.path());
        if (text.find("SagaServer/") != std::string::npos ||
            text.find("SagaEditor/") != std::string::npos ||
            text.find("SDE/") != std::string::npos ||
            text.find("SagaEngine/Server/") != std::string::npos ||
            text.find("SagaEngine/Editor/") != std::string::npos ||
            text.find("SagaEngine/Core/Private/") != std::string::npos)
        {
            offenders.push_back(RelativeToSourceRoot(entry.path()));
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "SagaDiagnostics public headers must not depend on Server, Editor, or SDE.";
}

TEST(PublicPrivateBoundaryTests, EnginePublicDoesNotExposeServerOrConcreteBackends)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto enginePublic = root / "Engine" / "Public";
    ASSERT_TRUE(std::filesystem::exists(enginePublic));

    EXPECT_FALSE(std::filesystem::exists(enginePublic / "SagaEngine" / "Server"))
        << "Server policy belongs under root Server/, not Engine/Public.";
    EXPECT_FALSE(std::filesystem::exists(enginePublic / "SagaEngine" / "Platform" / "SDL"))
        << "Concrete SDL platform classes must stay private behind PlatformFactory.";
    EXPECT_FALSE(std::filesystem::exists(enginePublic / "SagaEngine" / "Input" / "Backends" / "SDL"))
        << "Concrete SDL input classes must stay private behind IPlatformInputBackend factories.";
    EXPECT_FALSE(std::filesystem::exists(enginePublic / "SagaEngine" / "Render" / "Backend" / "Diligent"))
        << "Concrete render backend classes must stay private behind RenderBackendFactory.";
    EXPECT_FALSE(std::filesystem::exists(enginePublic / "SagaEngine" / "Resources" / "Formats" / "GltfMeshImporter.h"))
        << "glTF mesh import is a resource implementation detail, not public engine API.";

    const std::vector<std::string> forbiddenIncludes = {
        "SagaEngine/Server/",
        "SagaEngine/Platform/SDL/",
        "SagaEngine/Input/Backends/SDL/",
        "SagaEngine/Render/Backend/Diligent/",
    };

    std::vector<std::string> offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(enginePublic))
    {
        if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
        {
            continue;
        }

        const auto relative = RelativeToSourceRoot(entry.path()).generic_string();
        const auto lines = ReadLines(entry.path());
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            const auto& line = lines[i];
            if (line.find("#include") == std::string::npos)
            {
                continue;
            }

            for (const auto& forbidden : forbiddenIncludes)
            {
                if (line.find(forbidden) != std::string::npos)
                {
                    offenders.push_back(
                        relative + ":" + std::to_string(i + 1) + ": " + line);
                }
            }
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Engine public headers must not expose fixed server/backend paths. First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(PublicPrivateBoundaryTests, PublicRenderBackendSurfaceIsVendorNeutral)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const std::vector<std::filesystem::path> publicRoots = {
        root / "Engine" / "Public" / "SagaEngine" / "Render" / "Backend",
        root / "Engine" / "Public" / "SagaEngine" / "Graphics",
    };
    const std::vector<std::string> forbiddenTokens = {
        "Diligent",
        "Vk",
        "Vulkan",
        "ID3D",
        "D3D",
        "MTL",
        "Metal",
        "TheForge",
    };

    std::vector<std::string> offenders;
    for (const auto& publicRoot : publicRoots)
    {
        if (!std::filesystem::exists(publicRoot))
        {
            continue;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(publicRoot))
        {
            if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
            {
                continue;
            }

            const auto relative = RelativeToSourceRoot(entry.path()).generic_string();
            const auto lines = ReadLines(entry.path());
            for (std::size_t i = 0; i < lines.size(); ++i)
            {
                for (const auto& token : forbiddenTokens)
                {
                    if (Contains(lines[i], token))
                    {
                        offenders.push_back(
                            relative + ":" + std::to_string(i + 1) + ": " + token);
                    }
                }
            }
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Public render backend/graphics headers must remain vendor-neutral. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(
    PublicPrivateBoundaryTests,
    DiligentGraphicsBackendDoesNotInitializeDiligentDeviceFactory)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto backendRoot =
        root / "Engine" / "Private" / "SagaEngine" / "Graphics" /
        "Backends" / "Diligent";
    ASSERT_TRUE(std::filesystem::exists(backendRoot));

    const std::vector<std::string> forbiddenTokens = {
        "CreateDeviceAndContexts",
        "CreateDeviceAndSwapChain",
        "GetEngineFactory",
        "TryInitAPI",
    };

    std::vector<std::string> offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(backendRoot))
    {
        if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
        {
            continue;
        }

        const auto relative = RelativeToSourceRoot(entry.path()).generic_string();
        const auto lines = ReadLines(entry.path());
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            for (const auto& token : forbiddenTokens)
            {
                if (Contains(lines[i], token))
                {
                    offenders.push_back(
                        relative + ":" + std::to_string(i + 1) + ": " + token);
                }
            }
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "SagaGraphics Diligent adapter must borrow private device services "
        << "instead of initializing a second Diligent device. First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(PublicPrivateBoundaryTests, DiligentBackendSplitFilesStaySmall)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);

    struct Budget
    {
        std::filesystem::path path;
        std::size_t maxLines = 0;
    };

    const std::vector<Budget> budgets = {
        {
            root / "Engine" / "Private" / "SagaEngine" / "Render" /
                "Backend" / "Diligent" / "DiligentRenderBackend.cpp",
            700u,
        },
        {
            root / "Engine" / "Private" / "SagaEngine" / "Render" /
                "Backend" / "Diligent" / "DiligentRenderBackendSubmit.cpp",
            900u,
        },
        {
            root / "Engine" / "Private" / "SagaEngine" / "Render" /
                "Backend" / "Diligent" / "DiligentRenderBackendResources.cpp",
            500u,
        },
        {
            root / "Engine" / "Private" / "SagaEngine" / "Render" /
                "Backend" / "Diligent" / "DiligentRenderBackendFrame.cpp",
            500u,
        },
        {
            root / "Engine" / "Private" / "SagaEngine" / "Render" /
                "Backend" / "Diligent" / "DiligentRenderBackendOverlayApi.cpp",
            500u,
        },
        {
            root / "Engine" / "Private" / "SagaEngine" / "Render" /
                "Backend" / "Diligent" / "DiligentRenderBackendPrivate.h",
            260u,
        },
        {
            root / "Engine" / "Private" / "SagaEngine" / "Graphics" /
                "Backends" / "Diligent" / "DiligentGraphicsBackend.cpp",
            500u,
        },
        {
            root / "Engine" / "Private" / "SagaEngine" / "Graphics" /
                "Backends" / "Diligent" / "DiligentGraphicsBackendResources.cpp",
            500u,
        },
        {
            root / "Engine" / "Private" / "SagaEngine" / "Graphics" /
                "Backends" / "Diligent" / "DiligentGraphicsBackendBindings.cpp",
            500u,
        },
        {
            root / "Engine" / "Private" / "SagaEngine" / "Graphics" /
                "Backends" / "Diligent" / "DiligentGraphicsBackendFrame.cpp",
            500u,
        },
        {
            root / "Engine" / "Private" / "SagaEngine" / "Graphics" /
                "Backends" / "Diligent" / "DiligentGraphicsBackendDiagnostics.cpp",
            500u,
        },
        {
            root / "Engine" / "Private" / "SagaEngine" / "Graphics" /
                "Backends" / "Diligent" / "DiligentGraphicsBackend.h",
            350u,
        },
    };

    std::vector<std::string> offenders;
    for (const auto& budget : budgets)
    {
        ASSERT_TRUE(std::filesystem::exists(budget.path)) << budget.path;
        const auto lineCount = ReadLines(budget.path).size();
        if (lineCount > budget.maxLines)
        {
            offenders.push_back(
                RelativeToSourceRoot(budget.path).generic_string() + ": " +
                std::to_string(lineCount) + " > " +
                std::to_string(budget.maxLines));
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Diligent backend split files exceeded their line budgets. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(PublicPrivateBoundaryTests, DiligentRenderBackendPrivateHeaderStaysNarrow)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto privateHeader =
        root / "Engine" / "Private" / "SagaEngine" / "Render" /
        "Backend" / "Diligent" / "DiligentRenderBackendPrivate.h";
    ASSERT_TRUE(std::filesystem::exists(privateHeader)) << privateHeader;

    const auto text = ReadText(privateHeader);
    EXPECT_LE(ReadLines(privateHeader).size(), 260u)
        << "DiligentRenderBackendPrivate.h must stay a narrow split header";

    const std::vector<std::string_view> forbiddenTokens = {
        "ImGui",
        "ImDraw",
        "ImTextureID",
        "imgui.h",
        "static DiligentNativeResourceOwner",
        "inline DiligentNativeResourceOwner",
        "static DiligentGpuTimeline",
        "inline DiligentGpuTimeline",
        "static DiligentOverlayRenderer",
        "inline DiligentOverlayRenderer",
        "static DiligentFrameCapture",
        "inline DiligentFrameCapture",
        "static DiligentPipelineCache",
        "inline DiligentPipelineCache",
        "static Diligent::RefCntAutoPtr",
        "inline Diligent::RefCntAutoPtr",
        "static std::unique_ptr",
        "inline std::unique_ptr",
        "static std::shared_ptr",
        "inline std::shared_ptr",
    };

    std::vector<std::string> offenders;
    for (const auto token : forbiddenTokens)
    {
        if (Contains(text, token))
        {
            offenders.push_back(std::string(token));
        }
    }

    EXPECT_EQ(CountOccurrences(text, "inline bool g_verboseGPU"), 1u)
        << "Only the existing verbose GPU flag may remain as inline mutable state";
    EXPECT_TRUE(offenders.empty())
        << "DiligentRenderBackendPrivate.h gained forbidden private-header content. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());

    const auto publicRoot = root / "Engine" / "Public";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(publicRoot))
    {
        if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
        {
            continue;
        }

        EXPECT_FALSE(Contains(ReadText(entry.path()), "DiligentRenderBackendPrivate.h"))
            << RelativeToSourceRoot(entry.path())
            << " must not include the private render backend split header";
    }

    const auto cmakeRoot = root / "cmake";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(cmakeRoot))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".cmake")
        {
            continue;
        }

        EXPECT_FALSE(Contains(ReadText(entry.path()), "DiligentRenderBackendPrivate.h"))
            << RelativeToSourceRoot(entry.path())
            << " must not install or expose the private render backend split header";
    }
}

TEST(PublicPrivateBoundaryTests, DiligentBackendSplitResponsibilitiesStayPut)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto renderRoot =
        root / "Engine" / "Private" / "SagaEngine" / "Render" /
        "Backend" / "Diligent";
    const auto graphicsRoot =
        root / "Engine" / "Private" / "SagaEngine" / "Graphics" /
        "Backends" / "Diligent";

    const auto renderBackend =
        ReadText(renderRoot / "DiligentRenderBackend.cpp");
    const auto renderResources =
        ReadText(renderRoot / "DiligentRenderBackendResources.cpp");
    const auto renderFrame =
        ReadText(renderRoot / "DiligentRenderBackendFrame.cpp");
    const auto renderSubmit =
        ReadText(renderRoot / "DiligentRenderBackendSubmit.cpp");
    const auto renderOverlay =
        ReadText(renderRoot / "DiligentRenderBackendOverlayApi.cpp");
    const auto graphicsBackend =
        ReadText(graphicsRoot / "DiligentGraphicsBackend.cpp");
    const auto graphicsResources =
        ReadText(graphicsRoot / "DiligentGraphicsBackendResources.cpp");
    const auto graphicsBindings =
        ReadText(graphicsRoot / "DiligentGraphicsBackendBindings.cpp");
    const auto graphicsValidation =
        ReadText(graphicsRoot / "DiligentGraphicsBackendValidation.h");

    struct ExpectedSymbol
    {
        std::string_view fileRole;
        std::string_view text;
        std::string_view symbol;
    };

    const std::vector<ExpectedSymbol> required = {
        {"render resources", renderResources, "DiligentRenderBackend::CreateMesh"},
        {"render resources", renderResources, "DiligentRenderBackend::CreateMaterial"},
        {"render resources", renderResources, "DiligentRenderBackend::CreateTexture"},
        {"render frame", renderFrame, "DiligentRenderBackend::BeginFrame"},
        {"render frame", renderFrame, "DiligentRenderBackend::EndFrame"},
        {"render submit", renderSubmit, "DiligentRenderBackend::Submit"},
        {"render submit", renderSubmit, "CreateShadowDepthPSO"},
        {"render overlay", renderOverlay, "DiligentRenderBackend::InitOverlayRendering"},
        {"render overlay", renderOverlay, "DiligentRenderBackend::CaptureCurrentColorFrame"},
        {"graphics resources", graphicsResources, "DiligentGraphicsBackend::CreateTexture"},
        {"graphics resources", graphicsResources, "DiligentGraphicsBackend::DestroyTexture"},
        {"graphics bindings", graphicsBindings, "DiligentGraphicsBackend::CreateBindingLayout"},
        {"graphics bindings", graphicsBindings, "DiligentGraphicsBackend::CreateBindingSet"},
        {"graphics bindings", graphicsBindings, "DiligentGraphicsBackend::QueryBindingLayout"},
        {"graphics validation", graphicsValidation, "IsValidTextureDesc"},
        {"graphics validation", graphicsValidation, "EstimateTextureBytes"},
    };

    std::vector<std::string> offenders;
    for (const auto& item : required)
    {
        if (!Contains(item.text, item.symbol))
        {
            offenders.push_back(
                std::string(item.fileRole) + " missing " +
                std::string(item.symbol));
        }
    }

    const std::vector<std::string_view> renderRootForbidden = {
        "DiligentRenderBackend::CreateMesh",
        "DiligentRenderBackend::CreateMaterial",
        "DiligentRenderBackend::CreateTexture",
        "DiligentRenderBackend::BeginFrame",
        "DiligentRenderBackend::Submit",
        "DiligentRenderBackend::EndFrame",
        "DiligentRenderBackend::InitOverlayRendering",
        "DiligentRenderBackend::CaptureCurrentColorFrame",
        "CreateShadowDepthPSO",
    };
    for (const auto symbol : renderRootForbidden)
    {
        if (Contains(renderBackend, symbol))
        {
            offenders.push_back(
                "render root contains split symbol " + std::string(symbol));
        }
    }

    const std::vector<std::string_view> graphicsRootForbidden = {
        "DiligentGraphicsBackend::CreateTexture",
        "DiligentGraphicsBackend::CreateBuffer",
        "DiligentGraphicsBackend::CreateShader",
        "DiligentGraphicsBackend::CreatePipeline",
        "DiligentGraphicsBackend::CreateSampler",
        "DiligentGraphicsBackend::CreateBindingLayout",
        "DiligentGraphicsBackend::CreateBindingSet",
        "DiligentGraphicsBackend::DestroyTexture",
        "IsValidTextureDesc",
        "EstimateTextureBytes",
    };
    for (const auto symbol : graphicsRootForbidden)
    {
        if (Contains(graphicsBackend, symbol))
        {
            offenders.push_back(
                "graphics root contains split symbol " + std::string(symbol));
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Diligent backend split responsibilities drifted. First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(PublicPrivateBoundaryTests, GraphicsBindingContractStaysVendorNeutral)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const std::vector<std::filesystem::path> publicHeaders = {
        root / "Engine" / "Public" / "SagaEngine" / "Graphics" /
            "Bindings" / "GraphicsBindingTypes.h",
        root / "Engine" / "Public" / "SagaEngine" / "Graphics" /
            "Bindings" / "GraphicsBindingValidation.h",
        root / "Engine" / "Public" / "SagaEngine" / "Graphics" /
            "Handles" / "GraphicsHandle.h",
    };

    const std::vector<std::string> forbiddenTokens = {
        "Diligent",
        "IRenderDevice",
        "IDeviceContext",
        "IPipelineState",
        "IShaderResourceBinding",
        "IShaderResourceVariable",
        "ITextureView",
        "IBuffer",
        "RefCntAutoPtr",
        "ImGui",
        "ImDrawData",
        "ImTextureID",
        "Vulkan",
        "Vk",
        "ID3D",
        "native pointer",
    };

    std::vector<std::string> offenders;
    for (const auto& header : publicHeaders)
    {
        ASSERT_TRUE(std::filesystem::exists(header)) << header;
        const auto relative = RelativeToSourceRoot(header).generic_string();
        const auto lines = ReadLines(header);
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            for (const auto& token : forbiddenTokens)
            {
                if (Contains(lines[i], token))
                {
                    offenders.push_back(
                        relative + ":" + std::to_string(i + 1) + ": " + token);
                }
            }
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Public graphics binding contract must stay vendor-neutral. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());

    const auto handles =
        ReadText(root / "Engine" / "Public" / "SagaEngine" / "Graphics" /
                 "Handles" / "GraphicsHandle.h");
    EXPECT_TRUE(Contains(handles, "struct BindingLayoutHandle final : GraphicsHandle"));
    EXPECT_TRUE(Contains(handles, "struct BindingSetHandle final : GraphicsHandle"));
    EXPECT_FALSE(Contains(handles, "void*"));
    EXPECT_FALSE(Contains(handles, "uintptr_t"));
}

TEST(PublicPrivateBoundaryTests, M5ABindingImplementationDoesNotOwnNativeSrb)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto graphicsRoot =
        root / "Engine" / "Private" / "SagaEngine" / "Graphics" /
        "Backends" / "Diligent";
    const auto bindingImpl =
        ReadText(graphicsRoot / "DiligentGraphicsBackendBindings.cpp");
    const auto resourceImpl =
        ReadText(graphicsRoot / "DiligentGraphicsBackendResources.cpp");

    EXPECT_FALSE(Contains(bindingImpl, "CreateShaderResourceBinding"));
    EXPECT_FALSE(Contains(bindingImpl, "GetVariableByName"));
    EXPECT_FALSE(Contains(bindingImpl, "IShaderResourceBinding"));
    EXPECT_FALSE(Contains(bindingImpl, "IShaderResourceVariable"));
    EXPECT_FALSE(Contains(resourceImpl, "DiligentGraphicsBackend::CreateBindingLayout"));
    EXPECT_FALSE(Contains(resourceImpl, "DiligentGraphicsBackend::CreateBindingSet"));
}

TEST(PublicPrivateBoundaryTests, SagaGraphicsUmbrellaHeaderCompileSmoke)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto graphicsRoot =
        root / "Engine" / "Public" / "SagaEngine" / "Graphics";
    ASSERT_TRUE(std::filesystem::exists(graphicsRoot));
    ASSERT_TRUE(std::filesystem::exists(graphicsRoot / "Graphics.h"));

    SagaEngine::Graphics::TextureDesc texture{};
    texture.debugName = "architecture-texture";
    texture.width = 128u;
    texture.height = 64u;
    texture.usage =
        SagaEngine::Graphics::TextureUsageFlags::Sampled |
        SagaEngine::Graphics::TextureUsageFlags::RenderTarget;
    EXPECT_TRUE(SagaEngine::Graphics::HasFlag(
        texture.usage,
        SagaEngine::Graphics::TextureUsageFlags::Sampled));
    EXPECT_TRUE(SagaEngine::Graphics::HasFlag(
        texture.usage,
        SagaEngine::Graphics::TextureUsageFlags::RenderTarget));

    SagaEngine::Graphics::BufferDesc buffer{};
    buffer.sizeBytes = 4096u;
    buffer.usage = SagaEngine::Graphics::BufferUsage::Uniform;
    buffer.dynamic = true;

    SagaEngine::Graphics::PipelineDesc pipeline{};
    pipeline.colorFormat = SagaEngine::Graphics::ResourceFormat::Rgba16Float;
    pipeline.depthWrite = false;

    SagaEngine::Graphics::RenderBackendDesc backendDesc{};
    backendDesc.preferredBackend =
        SagaEngine::Graphics::BackendPreference::Compatibility;
    backendDesc.headless = true;

    SagaEngine::Graphics::SwapchainDesc swapchain{};
    swapchain.width = 320u;
    swapchain.height = 180u;
    swapchain.vsync = false;

    SagaEngine::Graphics::NullGraphicsBackend backend;
    EXPECT_TRUE(backend.Initialize(backendDesc, swapchain));
    backend.Resize(swapchain.width, swapchain.height);
    EXPECT_EQ(backend.Width(), 320u);
    EXPECT_EQ(backend.Height(), 180u);

    const auto textureHandle = backend.CreateTexture(texture);
    const auto bufferHandle = backend.CreateBuffer(buffer);
    const auto pipelineHandle = backend.CreatePipeline(pipeline);
    EXPECT_TRUE(textureHandle.IsValid());
    EXPECT_TRUE(bufferHandle.IsValid());
    EXPECT_TRUE(pipelineHandle.IsValid());

    const auto textureQuery = backend.QueryResource(
        SagaEngine::Graphics::GraphicsResourceKind::Texture,
        textureHandle);
    EXPECT_TRUE(textureQuery.live);
    EXPECT_EQ(textureQuery.debugName, "architecture-texture");

    SagaEngine::Graphics::GraphicsFrameResourceSetDesc frameResources{};
    frameResources.resourceClass =
        SagaEngine::Graphics::GraphicsFrameResourceClass::PerFrameTransient;
    frameResources.maxFramesInFlight = 2u;
    frameResources.bytesPerFrame = 1024u;
    EXPECT_EQ(frameResources.maxFramesInFlight, 2u);

    backend.BeginFrame();
    backend.EndFrame();

    auto status = backend.GetStatus();
    EXPECT_TRUE(status.initialized);
    EXPECT_EQ(
        status.selectedBackend,
        SagaEngine::Graphics::BackendPreference::Compatibility);
    EXPECT_EQ(status.frameIndex, 1u);
    EXPECT_EQ(
        status.health,
        SagaEngine::Graphics::RenderBackendHealth::Headless);
    EXPECT_EQ(
        status.failure,
        SagaEngine::Graphics::RenderBackendFailure::None);

    const auto capabilities = backend.GetCapabilities();
    EXPECT_EQ(
        capabilities.qualityCeiling,
        SagaEngine::Graphics::RenderQualityPreset::Low);
    EXPECT_EQ(
        capabilities.rayTracing,
        SagaEngine::Graphics::RenderFeatureSupport::Unsupported);
    EXPECT_EQ(
        SagaEngine::Graphics::ResolveRenderCapabilityFallback(
            capabilities.rayTracing,
            true),
        SagaEngine::Graphics::RenderCapabilityFallback::DisabledUnsupported);

    backend.DestroyTexture(textureHandle);
    backend.DestroyBuffer(bufferHandle);
    backend.DestroyPipeline(pipelineHandle);
    backend.Shutdown();

    status = backend.GetStatus();
    EXPECT_FALSE(status.initialized);
    EXPECT_EQ(
        status.health,
        SagaEngine::Graphics::RenderBackendHealth::Shutdown);
}

TEST(PublicPrivateBoundaryTests, RenderShaderMaterialCookContractsAreVendorNeutral)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const std::vector<std::filesystem::path> publicRoots = {
        root / "Engine" / "Public" / "SagaEngine" / "Render" / "Shaders",
        root / "Engine" / "Public" / "SagaEngine" / "Render" / "Materials",
        root / "Engine" / "Public" / "SagaEngine" / "Render" / "Assets",
    };
    const std::vector<std::string> forbiddenTokens = {
        "Diligent",
        "Vk",
        "Vulkan",
        "ID3D",
        "D3D",
        "MTL",
        "Metal",
    };

    std::vector<std::string> offenders;
    for (const auto& publicRoot : publicRoots)
    {
        ASSERT_TRUE(std::filesystem::exists(publicRoot)) << publicRoot;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(publicRoot))
        {
            if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
            {
                continue;
            }

            const auto relative = RelativeToSourceRoot(entry.path()).generic_string();
            const auto lines = ReadLines(entry.path());
            for (std::size_t i = 0; i < lines.size(); ++i)
            {
                for (const auto& token : forbiddenTokens)
                {
                    if (Contains(lines[i], token))
                    {
                        offenders.push_back(
                            relative + ":" + std::to_string(i + 1) + ": " + token);
                    }
                }
            }
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Shader/material/cook public contracts must remain vendor-neutral. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(PublicPrivateBoundaryTests, RenderStreamingResidencyContractIsVendorNeutral)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto publicRoot =
        root / "Engine" / "Public" / "SagaEngine" / "Render" / "Streaming";
    ASSERT_TRUE(std::filesystem::exists(publicRoot));

    const std::vector<std::string> forbiddenTokens = {
        "Diligent",
        "Vk",
        "Vulkan",
        "ID3D",
        "D3D",
        "MTL",
        "Metal",
        "native handle",
    };

    std::vector<std::string> offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(publicRoot))
    {
        if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
        {
            continue;
        }

        const auto relative = RelativeToSourceRoot(entry.path()).generic_string();
        const auto lines = ReadLines(entry.path());
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            for (const auto& token : forbiddenTokens)
            {
                if (Contains(lines[i], token))
                {
                    offenders.push_back(
                        relative + ":" + std::to_string(i + 1) + ": " + token);
                }
            }
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Render streaming residency public contracts must remain CPU-side "
        << "and backend-neutral. First offender: "
        << (offenders.empty() ? "" : offenders.front());

    SagaEngine::Render::Streaming::RenderTextureResidencyInput texture{};
    texture.assetId = 1u;
    texture.authoredMipCount = 1u;
    texture.availableMipRange = {.firstMip = 0u, .lastMip = 0u, .valid = true};
    SagaEngine::Render::Streaming::RenderTextureStreamingDecision decision{};
    decision.assetId = texture.assetId;
    decision.priority.kind =
        SagaEngine::Render::Streaming::RenderStreamingAssetKind::Texture;
    EXPECT_EQ(
        decision.priority.kind,
        SagaEngine::Render::Streaming::RenderStreamingAssetKind::Texture);
}

TEST(PublicPrivateBoundaryTests, RenderSceneAndInterestContractsAreVendorNeutral)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const std::vector<std::filesystem::path> publicHeaders = {
        root / "Engine" / "Public" / "SagaEngine" / "Render" / "Scene" /
            "RenderFrameSnapshot.h",
        root / "Engine" / "Public" / "SagaEngine" / "Render" / "World" /
            "RenderInterestWorldStreaming.h",
    };

    const std::vector<std::string> forbiddenTokens = {
        "Diligent",
        "Vk",
        "Vulkan",
        "ID3D",
        "D3D",
        "MTL",
        "Metal",
        "native handle",
    };

    std::vector<std::string> offenders;
    for (const auto& publicHeader : publicHeaders)
    {
        ASSERT_TRUE(std::filesystem::exists(publicHeader)) << publicHeader;
        const auto relative = RelativeToSourceRoot(publicHeader).generic_string();
        const auto lines = ReadLines(publicHeader);
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            for (const auto& token : forbiddenTokens)
            {
                if (Contains(lines[i], token))
                {
                    offenders.push_back(
                        relative + ":" + std::to_string(i + 1) + ": " + token);
                }
            }
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Render scene/interest public contracts must remain CPU-side "
        << "and backend-neutral. First offender: "
        << (offenders.empty() ? "" : offenders.front());

    SagaEngine::Render::Scene::RenderViewFamily family;
    family.views.push_back({
        .kind = SagaEngine::Render::Scene::RenderViewKind::Main,
        .viewIndex = 0u,
        .stableOrder = 1u,
    });
    EXPECT_EQ(family.views.size(), 1u);

    SagaEngine::Render::World::RenderInterestRecord record;
    record.interest =
        SagaEngine::Render::World::RenderInterestState::NetworkRelevant;
    EXPECT_EQ(
        record.visibility,
        SagaEngine::Render::World::RenderVisibilityState::Hidden);
}

TEST(PublicPrivateBoundaryTests, MaterialGraphAuthoringContractDocumentsNonClaims)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto contractDoc =
        root / "docs" / "architecture" /
        "RENDER_MATERIAL_GRAPH_AUTHORING_CONTRACT.md";
    ASSERT_TRUE(std::filesystem::exists(contractDoc));

    const auto text = ReadText(contractDoc);
    const std::vector<std::string> requiredTokens = {
        "MaterialGraphOutputSchema",
        "ShaderVariantBudgetConfig",
        "MaterialAsset",
        "does not implement an editor graph canvas",
        "does not compile shaders",
        "does not update native descriptor bindings",
        "does not prove a PBR material render path or golden image",
        "does not implement shader or material hot reload",
    };

    for (const auto& token : requiredTokens)
    {
        EXPECT_TRUE(Contains(text, token))
            << "Material graph authoring contract must document: " << token;
    }

    const std::vector<std::string> forbiddenClaims = {
        "rendered triangle",
        "full material renderer",
        "hot reload complete",
        "production AAA renderer",
    };

    for (const auto& claim : forbiddenClaims)
    {
        EXPECT_FALSE(Contains(text, claim))
            << "Material graph authoring contract must not claim: " << claim;
    }
}

TEST(PublicPrivateBoundaryTests, RenderPublicApiContractDocumentsGraphicsGuardrails)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto contractDoc =
        root / "docs" / "architecture" / "RENDER_PUBLIC_API_CONTRACT.md";
    ASSERT_TRUE(std::filesystem::exists(contractDoc));

    const auto text = ReadText(contractDoc);
    const std::vector<std::string> requiredTokens = {
        "SagaEngine/Graphics",
        "SagaGraphics",
        "Engine/Public",
        "VendorDiligent",
        "SagaDiligentBackend",
        "RenderBackendHealth",
        "RenderBackendFailure",
        "RenderBackendCapabilities",
        "RenderQualityPreset",
        "does not complete R3C native feature detection or capability artifacts",
        "stable external SDK",
        "does not move `SagaEngine/Render/Backend`",
        "does not complete R3 bridge migration",
        "R5 validation entry adds RenderGraph compile diagnostics",
        "does not add a SagaGraphics execution bridge",
        "material system, shader compiler, lighting, or post-processing",
        "does not complete R3B device-loss or swapchain recreation recovery",
        "does not create native Diligent GPU resources",
        "does not complete R5 RenderGraph execution",
        "CPU-side render residency foundation",
        "does not implement GPU texture upload",
        "does not implement virtual texture residency",
        "does not prove terrain or mesh streaming render smoke",
        "CPU-side scene extraction and view-family foundation",
        "does not implement a dedicated render thread",
        "does not implement multi-view render targets",
        "does not make network relevance the same thing as render relevance",
    };

    for (const auto& token : requiredTokens)
    {
        EXPECT_TRUE(Contains(text, token))
            << "RENDER_PUBLIC_API_CONTRACT.md must document: " << token;
    }

    const std::vector<std::string> forbiddenClaims = {
        "production AAA renderer",
        "full R5 completed",
        "R5 completed",
    };

    for (const auto& claim : forbiddenClaims)
    {
        EXPECT_FALSE(Contains(text, claim))
            << "RENDER_PUBLIC_API_CONTRACT.md must not claim: " << claim;
    }
}

TEST(PublicPrivateBoundaryTests, RenderStreamingDocsDoNotOverclaimGpuStreaming)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const std::vector<std::filesystem::path> docs = {
        root / "TEMP.MD",
        root / "docs" / "architecture" / "RENDER_PUBLIC_API_CONTRACT.md",
    };

    const std::vector<std::string> forbiddenClaims = {
        "virtual texture implemented",
        "terrain render smoke passed",
        "GPU streaming complete",
        "full texture streaming complete",
        "full mesh streaming complete",
    };

    for (const auto& doc : docs)
    {
        ASSERT_TRUE(std::filesystem::exists(doc)) << doc;
        const auto text = ReadText(doc);
        for (const auto& claim : forbiddenClaims)
        {
            EXPECT_FALSE(Contains(text, claim))
                << RelativeToSourceRoot(doc).generic_string()
                << " must not claim: " << claim;
        }
    }
}

TEST(PublicPrivateBoundaryTests, RenderInterestWorldStreamingContractDocumentsNonClaims)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto contractDoc =
        root / "docs" / "architecture" /
        "RENDER_INTEREST_WORLD_STREAMING_CONTRACT.md";
    ASSERT_TRUE(std::filesystem::exists(contractDoc));

    const auto text = ReadText(contractDoc);
    const std::vector<std::string> requiredTokens = {
        "Network relevance",
        "Client streaming relevance",
        "Render visibility",
        "Render resource residency",
        "does not implement a terrain renderer",
        "does not prove unloaded-zone fallback render smoke",
        "does not make network relevance the same thing as render relevance",
        "does not replace `Resources::StreamingManager`",
        "does not complete GPU streaming behavior",
        "does not implement virtual texture residency",
    };

    for (const auto& token : requiredTokens)
    {
        EXPECT_TRUE(Contains(text, token))
            << "Render interest contract must document: " << token;
    }

    const std::vector<std::string> forbiddenClaims = {
        "terrain renderer complete",
        "multi-view render targets implemented",
        "render thread complete",
        "GPU streaming complete",
        "production renderer",
    };

    for (const auto& claim : forbiddenClaims)
    {
        EXPECT_FALSE(Contains(text, claim))
            << "Render interest contract must not claim: " << claim;
    }
}

TEST(PublicPrivateBoundaryTests, GraphicsPrivateRenderTestsUsePrivateStyleIncludes)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const std::vector<std::pair<std::filesystem::path, std::string>> testFiles = {
        {
            root / "Tests" / "Unit" / "Render" /
                "GraphicsDiligentBackendTestHelpers.h",
            "#include \"SagaEngine/Graphics/Backends/Diligent/"
            "DiligentGraphicsBackend.h\"",
        },
        {
            root / "Tests" / "Unit" / "Render" /
                "GraphicsDiligentBackendLifecycleTests.cpp",
            "#include \"GraphicsDiligentBackendTestHelpers.h\"",
        },
        {
            root / "Tests" / "Unit" / "Render" /
                "GraphicsDiligentBackendBindingTests.cpp",
            "#include \"GraphicsDiligentBackendTestHelpers.h\"",
        },
        {
            root / "Tests" / "Unit" / "Render" /
                "GraphicsDiligentBackendResourceRegistryTests.cpp",
            "#include \"GraphicsDiligentBackendTestHelpers.h\"",
        },
        {
            root / "Tests" / "Unit" / "Render" /
                "GraphicsDiligentBackendNativeBufferTests.cpp",
            "#include \"GraphicsDiligentBackendTestHelpers.h\"",
        },
        {
            root / "Tests" / "Unit" / "Render" /
                "GraphicsDiligentBackendNativeTextureTests.cpp",
            "#include \"GraphicsDiligentBackendTestHelpers.h\"",
        },
        {
            root / "Tests" / "Unit" / "Render" /
                "GraphicsDiligentBackendNativeSamplerTests.cpp",
            "#include \"GraphicsDiligentBackendTestHelpers.h\"",
        },
        {
            root / "Tests" / "Unit" / "Render" /
                "GraphicsDiligentBackendNativeShaderPipelineTests.cpp",
            "#include \"GraphicsDiligentBackendTestHelpers.h\"",
        },
        {
            root / "Tests" / "Unit" / "Render" /
                "GraphicsRenderBackendMappingTests.cpp",
            "#include \"SagaEngine/Graphics/Backend/"
            "GraphicsRenderBackendMapping.h\"",
        },
    };

    for (const auto& [path, expectedInclude] : testFiles)
    {
        ASSERT_TRUE(std::filesystem::exists(path)) << path;
        const auto text = ReadText(path);
        EXPECT_TRUE(Contains(text, expectedInclude))
            << RelativeToSourceRoot(path)
            << " must include private graphics headers through the "
               "SagaEngine/Graphics private-style include root";
        EXPECT_FALSE(Contains(text, "../../../Engine/Private/"))
            << RelativeToSourceRoot(path)
            << " must not include Engine/Private by relative path";
        EXPECT_FALSE(Contains(text, "#include \"../../../"))
            << RelativeToSourceRoot(path)
            << " must not use parent-directory private include traversal";
        EXPECT_FALSE(Contains(text, "Private/SagaEngine/Graphics/"))
            << RelativeToSourceRoot(path)
            << " must not mention Private/ in include directives";
    }
}

TEST(PublicPrivateBoundaryTests, NormalFramePathDoesNotUseGlobalDeviceIdle)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto diligentDir = root / "Engine" / "Private" / "SagaEngine" /
        "Render" / "Backend" / "Diligent";
    const auto framePath = diligentDir / "DiligentRenderBackendFrame.cpp";
    const auto submitPath = diligentDir / "DiligentRenderBackendSubmit.cpp";
    const auto overlayPath = diligentDir / "DiligentRenderBackendOverlayApi.cpp";
    ASSERT_TRUE(std::filesystem::exists(diligentDir)) << diligentDir;
    ASSERT_TRUE(std::filesystem::exists(framePath)) << framePath;
    ASSERT_TRUE(std::filesystem::exists(submitPath)) << submitPath;
    ASSERT_TRUE(std::filesystem::exists(overlayPath)) << overlayPath;

    const auto frameText = ReadText(framePath);
    const auto submitText = ReadText(submitPath);
    const auto overlayText = ReadText(overlayPath);
    const auto beginFrame = SliceBetween(
        frameText,
        "void DiligentRenderBackend::BeginFrame()",
        "void DiligentRenderBackend::EndFrame()");
    const auto submit = SliceBetween(
        submitText,
        "void DiligentRenderBackend::Submit(",
        "} // namespace SagaEngine::Render::Backend");
    const auto endFrame = SliceBetween(
        frameText,
        "void DiligentRenderBackend::EndFrame()",
        "} // namespace SagaEngine::Render::Backend");

    ASSERT_FALSE(beginFrame.empty());
    ASSERT_FALSE(submit.empty());
    ASSERT_FALSE(endFrame.empty());
    EXPECT_FALSE(Contains(beginFrame, "WaitForIdle("));
    EXPECT_FALSE(Contains(submit, "WaitForIdle("));
    EXPECT_FALSE(Contains(endFrame, "WaitForIdle("));
    EXPECT_TRUE(Contains(overlayText, "CaptureCurrentColorFrame"));
}

TEST(PublicPrivateBoundaryTests, DiligentRenderBackendDoesNotDependOnImGui)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto diligentDir = root / "Engine" / "Private" / "SagaEngine" /
        "Render" / "Backend" / "Diligent";
    const auto backendCpp = diligentDir / "DiligentRenderBackend.cpp";
    const auto backendH = diligentDir / "DiligentRenderBackend.h";
    const auto publicFactory = root / "Engine" / "Public" / "SagaEngine" /
        "Render" / "Backend" / "RenderBackendFactory.h";

    ASSERT_TRUE(std::filesystem::exists(backendCpp)) << backendCpp;
    ASSERT_TRUE(std::filesystem::exists(backendH)) << backendH;
    ASSERT_TRUE(std::filesystem::exists(publicFactory)) << publicFactory;

    const std::array paths{backendCpp, backendH, publicFactory};
    for (const auto& path : paths)
    {
        const auto text = ReadText(path);
        EXPECT_FALSE(Contains(text, "imgui.h")) << RelativeToSourceRoot(path);
        EXPECT_FALSE(Contains(text, "ImGui")) << RelativeToSourceRoot(path);
        EXPECT_FALSE(Contains(text, "ImDraw")) << RelativeToSourceRoot(path);
        EXPECT_FALSE(Contains(text, "ImTextureID")) << RelativeToSourceRoot(path);
    }
}

TEST(PublicPrivateBoundaryTests, OldImGuiRenderApiSymbolsAreRemoved)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const std::array<std::string, 3> oldSymbols = {
        std::string("InitBackend") + "ImGuiRendering",
        std::string("RenderBackend") + "ImGuiDrawData",
        std::string("ShutdownBackend") + "ImGuiRendering",
    };

    std::vector<std::string> offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
        {
            continue;
        }

        const auto relative = RelativeToSourceRoot(entry.path()).generic_string();
        if (relative == "Tests/Unit/Architecture/PublicPrivateBoundaryTests.cpp")
        {
            continue;
        }
        const auto text = ReadText(entry.path());
        for (const auto& symbol : oldSymbols)
        {
            if (Contains(text, symbol))
            {
                offenders.push_back(relative + ": " + symbol);
            }
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Old public ImGui render API symbols must stay removed. First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(PublicPrivateBoundaryTests, PublicRenderHeadersContainNoImGuiTypes)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto renderPublic =
        root / "Engine" / "Public" / "SagaEngine" / "Render";
    ASSERT_TRUE(std::filesystem::exists(renderPublic));

    const std::array<std::string_view, 4> forbidden = {
        "ImGui",
        "ImDraw",
        "ImTextureID",
        "imgui.h",
    };

    std::vector<std::string> offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(renderPublic))
    {
        if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
        {
            continue;
        }

        const auto relative = RelativeToSourceRoot(entry.path()).generic_string();
        const auto text = ReadText(entry.path());
        for (const auto token : forbidden)
        {
            if (Contains(text, token))
            {
                offenders.push_back(relative + ": " + std::string(token));
            }
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Public render headers must not expose ImGui types. First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(PublicPrivateBoundaryTests, AllRepositoryCallsitesUseOverlayApi)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const std::array<std::string_view, 5> requiredSymbols = {
        "InitBackendOverlayRendering",
        "RenderBackendOverlayFrame",
        "ShutdownBackendOverlayRendering",
        "CreateBackendOverlayTexture",
        "DestroyBackendOverlayTexture",
    };
    const auto factory = root / "Engine" / "Public" / "SagaEngine" /
        "Render" / "Backend" / "RenderBackendFactory.h";
    const auto factoryText = ReadText(factory);

    for (const auto symbol : requiredSymbols)
    {
        EXPECT_TRUE(Contains(factoryText, symbol))
            << "Missing public overlay API: " << symbol;
    }

    const auto sandboxAdapter = root / "Apps" / "Sandbox" / "src" / "UI" /
        "SandboxImGuiOverlayAdapter.cpp";
    ASSERT_TRUE(std::filesystem::exists(sandboxAdapter));
    const auto sandboxText = ReadText(sandboxAdapter);
    EXPECT_TRUE(Contains(sandboxText, "InitBackendOverlayRendering"));
    EXPECT_TRUE(Contains(sandboxText, "RenderBackendOverlayFrame"));
    EXPECT_TRUE(Contains(sandboxText, "CreateBackendOverlayTexture"));
    EXPECT_TRUE(Contains(sandboxText, "DestroyBackendOverlayTexture"));
}

TEST(PublicPrivateBoundaryTests, SandboxUsesOnlyPublicOverlayApi)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto sandboxUi =
        root / "Apps" / "Sandbox" / "include" / "SagaSandbox" / "UI";
    const auto sandboxSrc = root / "Apps" / "Sandbox" / "src" / "UI";

    std::vector<std::string> offenders;
    for (const auto& base : {sandboxUi, sandboxSrc})
    {
        ASSERT_TRUE(std::filesystem::exists(base)) << base;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(base))
        {
            if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
            {
                continue;
            }

            const auto relative = RelativeToSourceRoot(entry.path()).generic_string();
            const auto text = ReadText(entry.path());
            if (Contains(text, "SagaEngine/Render/Backend/Diligent/") ||
                Contains(text, "DiligentOverlayRenderer") ||
                Contains(text, "DiligentRenderBackend"))
            {
                offenders.push_back(relative);
            }
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Sandbox UI must reach overlay rendering only through public API. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(PublicPrivateBoundaryTests, SimulationPublicDoesNotIncludeInputNetworking)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto simulationPublic = root / "Engine" / "Public" / "SagaEngine" / "Simulation";
    ASSERT_TRUE(std::filesystem::exists(simulationPublic));

    std::vector<std::string> offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(simulationPublic))
    {
        if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
        {
            continue;
        }

        const auto relative = RelativeToSourceRoot(entry.path()).generic_string();
        const auto lines = ReadLines(entry.path());
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            const auto& line = lines[i];
            if (line.find("#include") != std::string::npos &&
                line.find("SagaEngine/Input/Networking/") != std::string::npos)
            {
                offenders.push_back(
                    relative + ":" + std::to_string(i + 1) + ": " + line);
            }
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Simulation public headers must not depend on input networking. First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(PublicPrivateBoundaryTests, EnginePublicInternalCandidateBucketsStayDocumented)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto enginePublic = root / "Engine" / "Public" / "SagaEngine";
    const auto auditDoc =
        root / "docs" / "architecture" / "ENGINE_PUBLIC_API_AUDIT.md";
    ASSERT_TRUE(std::filesystem::exists(enginePublic));
    ASSERT_TRUE(std::filesystem::exists(auditDoc));

    const auto auditText = ReadText(auditDoc);
    const std::vector<std::string> internalCandidateBuckets = {
        "SagaEngine/World",
        "SagaEngine/Render/RenderGraph",
        "SagaEngine/Render/RenderPasses",
        "SagaEngine/RHI/Types",
        "SagaEngine/Client/Replication",
        "SagaEngine/Scripting/LowLevel",
        "SagaEngine/Scripting/Authoring",
    };

    for (const auto& bucket : internalCandidateBuckets)
    {
        const auto bucketPath = std::filesystem::path(bucket);
        const auto publicPath =
            enginePublic / bucketPath.lexically_relative("SagaEngine");
        EXPECT_TRUE(std::filesystem::exists(publicPath))
            << bucket << " is tracked as public internal-candidate surface.";
        EXPECT_TRUE(Contains(auditText, bucket))
            << bucket << " must stay classified in ENGINE_PUBLIC_API_AUDIT.md.";
    }

    const std::vector<std::string> diagnosticsTokens = {
        "*Record",
        "*Snapshot",
        "*Tracker",
        "*Builder",
    };

    for (const auto& token : diagnosticsTokens)
    {
        EXPECT_TRUE(Contains(auditText, token))
            << "Diagnostics internal-candidate token " << token
            << " must stay classified in ENGINE_PUBLIC_API_AUDIT.md.";
    }
}

TEST(PublicPrivateBoundaryTests, WorldFacadePublicShellDoesNotExposeInternalWorldTypes)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto facadeHeader =
        root / "Engine" / "Public" / "SagaEngine" / "World" / "WorldFacade.h";
    ASSERT_TRUE(std::filesystem::exists(facadeHeader));

    const auto text = ReadText(facadeHeader);
    const std::vector<std::string> forbiddenTokens = {
        "SimCell",
        "SimCellId",
        "DomainScheduler",
        "EventStream",
        "RelevanceGraph",
        "WorldState",
        "ClientSession",
        "WorldEvent",
        "WorldSnapshot",
        "RelevanceResult",
        "ComponentMask",
        "EntityDirtyState",
        "DomainTickDispatcher",
    };

    std::vector<std::string> offenders;
    for (const auto& token : forbiddenTokens)
    {
        if (Contains(text, token))
        {
            offenders.push_back(token);
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "WorldFacade.h must not expose internal World cluster tokens. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());

    SagaEngine::World::WorldFacade facade;
    const SagaEngine::World::WorldFacadeConfig config{};
    EXPECT_TRUE(facade.Initialize(config));

    const auto tick = facade.Tick({});
    EXPECT_TRUE(tick.initialized);
    EXPECT_TRUE(tick.accepted);
    EXPECT_EQ(tick.tickIndex, 1u);

    const auto status = facade.GetStatus();
    EXPECT_TRUE(status.initialized);
    EXPECT_EQ(status.tickIndex, 1u);

    const auto commandAccepted = facade.SubmitCommand({});
    EXPECT_TRUE(commandAccepted);

    const auto stats = facade.GetStats();
    EXPECT_EQ(stats.ticksSubmitted, 1u);
    EXPECT_EQ(stats.commandsSubmitted, 1u);

    facade.Shutdown();
    EXPECT_FALSE(facade.GetStatus().initialized);
}

TEST(PublicPrivateBoundaryTests, RenderPipelineConfigPublicShellDoesNotExposeRenderInternals)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto configHeader =
        root / "Engine" / "Public" / "SagaEngine" / "Render" /
        "RenderPipelineConfig.h";
    ASSERT_TRUE(std::filesystem::exists(configHeader));

    SagaEngine::Render::RenderPipelineConfig config;
    config.profile = SagaEngine::Render::RenderPipelineProfile::Forward;
    config.features.shadows = true;
    config.features.postProcessing = true;
    config.features.highDynamicRange = true;
    config.features.bloom = true;
    config.features.antiAliasing = true;
    config.quality.quality =
        SagaEngine::Render::RenderPipelineQualityLevel::High;
    config.quality.renderScale = 1.0f;
    config.quality.lodBias = 1.0f;
    config.quality.maxDrawItemsPerView = 8192u;
    config.quality.shadowMapResolution = 2048u;
    config.quality.antiAliasingSamples = 4u;
    config.postProcess.enabled = true;
    config.postProcess.quality =
        SagaEngine::Render::RenderPostProcessQuality::High;
    config.postProcess.outputFormat =
        SagaEngine::Render::RenderPostProcessOutputFormat::HighDynamicRange;
    config.postProcess.antiAliasing =
        SagaEngine::Render::RenderAntiAliasingMode::Temporal;
    config.postProcess.depthOfField = true;
    config.postProcess.motionBlur = true;
    config.postProcess.sharpening = true;
    config.postProcess.vignette = true;
    config.postProcess.filmGrain = true;
    config.postProcess.chromaticAberration = true;
    config.postProcess.temporalAntiAliasing.enabled = true;
    config.postProcess.temporalAntiAliasing.quality =
        SagaEngine::Render::RenderPostProcessQuality::Ultra;
    config.postProcess.temporalAntiAliasing.historyWeight = 0.85f;
    config.postProcess.temporalAntiAliasing.jitterScale = 0.75f;
    config.postProcess.bloom.enabled = true;
    config.postProcess.bloom.quality =
        SagaEngine::Render::RenderPostProcessQuality::Medium;
    config.postProcess.bloom.threshold = 1.25f;
    config.postProcess.bloom.softKnee = 0.4f;
    config.postProcess.bloom.intensity = 0.08f;
    config.postProcess.bloom.mipCount = 5u;
    config.postProcess.toneMap.enabled = true;
    config.postProcess.toneMap.op =
        SagaEngine::Render::RenderToneMapOperator::AgX;
    config.postProcess.toneMap.exposureCompensationEV = 0.25f;
    config.postProcess.toneMap.whitePoint = 10.0f;
    config.postProcess.exposure.enabled = true;
    config.postProcess.exposure.mode =
        SagaEngine::Render::RenderExposureMode::Automatic;
    config.postProcess.exposure.minEv100 = -2.0f;
    config.postProcess.exposure.maxEv100 = 14.0f;
    config.postProcess.exposure.adaptationSpeed = 1.4f;
    config.postProcess.exposure.meterKey = 0.2f;
    config.postProcess.colorGrading.enabled = true;
    config.postProcess.colorGrading.style =
        SagaEngine::Render::RenderColorGradingStyle::Filmic;
    config.postProcess.colorGrading.intensity = 0.9f;
    config.postProcess.colorGrading.saturation = 1.1f;
    config.postProcess.colorGrading.contrast = 1.2f;
    config.shadows.enabled = true;
    config.shadows.quality = SagaEngine::Render::RenderShadowQuality::Ultra;
    config.shadows.atlasSize = 8192u;
    config.shadows.maxCasters = 48u;
    config.shadows.cascadeCount = 4u;
    config.shadows.cascadeLambda = 0.65f;
    config.shadows.maxDistance = 240.0f;
    config.shadows.bias.depthBias0 = 0.0004f;
    config.shadows.bias.depthBias1 = 0.0008f;
    config.shadows.bias.depthBias2 = 0.0020f;
    config.shadows.bias.depthBias3 = 0.0040f;
    config.shadows.bias.slopeScaleBias0 = 1.1f;
    config.shadows.bias.slopeScaleBias1 = 1.6f;
    config.shadows.bias.slopeScaleBias2 = 2.7f;
    config.shadows.bias.slopeScaleBias3 = 4.2f;
    config.debug.captureStats = true;
    config.debug.deterministicOrdering = true;
    config.maxViews = 2u;
    config.enabled = true;

    EXPECT_EQ(config.profile, SagaEngine::Render::RenderPipelineProfile::Forward);
    EXPECT_TRUE(config.features.bloom);
    EXPECT_EQ(config.quality.antiAliasingSamples, 4u);
    EXPECT_EQ(
        config.postProcess.outputFormat,
        SagaEngine::Render::RenderPostProcessOutputFormat::HighDynamicRange);
    EXPECT_EQ(
        config.postProcess.antiAliasing,
        SagaEngine::Render::RenderAntiAliasingMode::Temporal);
    EXPECT_TRUE(config.postProcess.temporalAntiAliasing.enabled);
    EXPECT_FLOAT_EQ(
        config.postProcess.temporalAntiAliasing.historyWeight,
        0.85f);
    EXPECT_TRUE(config.postProcess.bloom.enabled);
    EXPECT_EQ(config.postProcess.bloom.mipCount, 5u);
    EXPECT_EQ(
        config.postProcess.toneMap.op,
        SagaEngine::Render::RenderToneMapOperator::AgX);
    EXPECT_FLOAT_EQ(config.postProcess.exposure.meterKey, 0.2f);
    EXPECT_EQ(
        config.postProcess.colorGrading.style,
        SagaEngine::Render::RenderColorGradingStyle::Filmic);
    EXPECT_EQ(config.shadows.quality, SagaEngine::Render::RenderShadowQuality::Ultra);
    EXPECT_EQ(config.shadows.atlasSize, 8192u);
    EXPECT_EQ(config.shadows.maxCasters, 48u);
    EXPECT_EQ(config.shadows.cascadeCount, 4u);
    EXPECT_FLOAT_EQ(config.shadows.cascadeLambda, 0.65f);
    EXPECT_FLOAT_EQ(config.shadows.maxDistance, 240.0f);
    EXPECT_FLOAT_EQ(config.shadows.bias.depthBias3, 0.0040f);
    EXPECT_FLOAT_EQ(config.shadows.bias.slopeScaleBias3, 4.2f);
    EXPECT_TRUE(config.debug.captureStats);
    EXPECT_EQ(config.maxViews, 2u);
    EXPECT_TRUE(config.enabled);

    SagaEngine::Render::RenderPipelineStatus status;
    status.status = SagaEngine::Render::RenderPipelineStatusCode::Ready;
    status.accepted = true;
    status.active = true;
    status.warningCount = 1u;
    status.unsupportedFeatureCount = 0u;

    EXPECT_EQ(status.status, SagaEngine::Render::RenderPipelineStatusCode::Ready);
    EXPECT_TRUE(status.accepted);
    EXPECT_TRUE(status.active);
    EXPECT_EQ(status.warningCount, 1u);
    EXPECT_EQ(status.unsupportedFeatureCount, 0u);

    SagaEngine::Render::RenderPipelineStats stats;
    stats.framesSubmitted = 3u;
    stats.activeViews = 2u;
    stats.drawItemCount = 128u;
    stats.culledItemCount = 16u;
    stats.cpuFrameMilliseconds = 1.5f;
    stats.gpuFrameMilliseconds = 2.5f;

    EXPECT_EQ(stats.framesSubmitted, 3u);
    EXPECT_EQ(stats.activeViews, 2u);
    EXPECT_EQ(stats.drawItemCount, 128u);
    EXPECT_EQ(stats.culledItemCount, 16u);
    EXPECT_FLOAT_EQ(stats.cpuFrameMilliseconds, 1.5f);
    EXPECT_FLOAT_EQ(stats.gpuFrameMilliseconds, 2.5f);

    const auto text = ReadText(configHeader);
    const std::vector<std::string> forbiddenTokens = {
        "RenderGraph",
        "RGPass",
        "RGCompilation",
        "RGCompiler",
        "CompiledGraph",
        "FrameGraph",
        "FrameGraphExecutor",
        "GBufferPass",
        "LightingPass",
        "PostProcessGraph",
        "ShadowMap",
        "MaterialVec4",
        "ShadowPassDescriptor",
        "ShadowAtlasRect",
        "RHI::",
        "RG",
        "RenderPasses",
        "RenderPass",
        "IRHI",
        "CommandRecorder",
        "CommandBuffer",
        "TransientResource",
        "RenderWorld",
        "Renderer",
    };

    std::vector<std::string> offenders;
    for (const auto& token : forbiddenTokens)
    {
        if (Contains(text, token))
        {
            offenders.push_back(token);
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "RenderPipelineConfig.h must not expose render implementation tokens. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(PublicPrivateBoundaryTests, LowRiskRenderImplementationHeadersStayPrivate)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto enginePublic =
        root / "Engine" / "Public" / "SagaEngine" / "Render";
    const auto enginePrivate =
        root / "Engine" / "Private" / "SagaEngine" / "Render";
    ASSERT_TRUE(std::filesystem::exists(enginePublic));
    ASSERT_TRUE(std::filesystem::exists(enginePrivate));

    EXPECT_FALSE(std::filesystem::exists(
        enginePublic / "RenderGraph" / "RGCompilation.h"));
    EXPECT_FALSE(std::filesystem::exists(
        enginePublic / "RenderPasses" / "GBufferPass.h"));
    EXPECT_FALSE(std::filesystem::exists(
        enginePublic / "RenderPasses" / "LightingPass.h"));

    EXPECT_TRUE(std::filesystem::exists(
        enginePrivate / "RenderGraph" / "RGCompilation.h"));
    EXPECT_TRUE(std::filesystem::exists(
        enginePrivate / "RenderPasses" / "GBufferPass.h"));
    EXPECT_TRUE(std::filesystem::exists(
        enginePrivate / "RenderPasses" / "LightingPass.h"));

    const std::vector<std::string> forbiddenTokens = {
        "RGCompilation.h",
        "RGCompiler",
        "CompiledGraph",
        "GBufferPass",
        "LightingPass",
        "RenderPasses/GBufferPass.h",
        "RenderPasses/LightingPass.h",
    };

    std::vector<std::string> offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(enginePublic))
    {
        if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
        {
            continue;
        }

        const auto relative = RelativeToSourceRoot(entry.path()).generic_string();
        const auto lines = ReadLines(entry.path());
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            for (const auto& token : forbiddenTokens)
            {
                if (Contains(lines[i], token))
                {
                    offenders.push_back(
                        relative + ":" + std::to_string(i + 1) + ": " + token);
                }
            }
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Public render headers must not expose moved implementation tokens. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(PublicPrivateBoundaryTests, RenderFrameExecutionHeadersStayPrivate)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto enginePublic =
        root / "Engine" / "Public" / "SagaEngine" / "Render";
    const auto enginePrivate =
        root / "Engine" / "Private" / "SagaEngine" / "Render";
    ASSERT_TRUE(std::filesystem::exists(enginePublic));
    ASSERT_TRUE(std::filesystem::exists(enginePrivate));

    EXPECT_FALSE(std::filesystem::exists(
        enginePublic / "FrameGraphExecutor.h"));
    EXPECT_FALSE(std::filesystem::exists(
        enginePublic / "CommandRecording" / "CommandBuffer.h"));
    EXPECT_FALSE(std::filesystem::exists(
        enginePublic / "CommandRecording" / "CommandRecorder.h"));
    EXPECT_FALSE(std::filesystem::exists(
        enginePublic / "Renderer.h"));

    EXPECT_TRUE(std::filesystem::exists(
        enginePrivate / "FrameGraphExecutor.h"));
    EXPECT_TRUE(std::filesystem::exists(
        enginePrivate / "CommandRecording" / "CommandBuffer.h"));
    EXPECT_TRUE(std::filesystem::exists(
        enginePrivate / "CommandRecording" / "CommandRecorder.h"));
    EXPECT_TRUE(std::filesystem::exists(
        enginePrivate / "Renderer.h"));

    const std::vector<std::string> forbiddenTokens = {
        "FrameGraphExecutor.h",
        "FrameGraphExecutor",
        "CommandRecorder.h",
        "CommandRecorder",
        "CommandBuffer.h",
        "CommandBuffer",
        "Renderer.h",
        "class Renderer",
    };

    std::vector<std::string> offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(enginePublic))
    {
        if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
        {
            continue;
        }

        const auto relative = RelativeToSourceRoot(entry.path()).generic_string();
        const auto lines = ReadLines(entry.path());
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            for (const auto& token : forbiddenTokens)
            {
                if (Contains(lines[i], token))
                {
                    offenders.push_back(
                        relative + ":" + std::to_string(i + 1) + ": " + token);
                }
            }
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Public render headers must not expose frame execution or command "
        << "recording implementation tokens. First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(PublicPrivateBoundaryTests, RmlUiHeadersStayInsideRmlUiBackendImplementation)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const std::string allowedPrefix = "Backends/src/UI/RmlUi/";

    std::vector<std::string> offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file() || !IsCodeFile(entry.path()))
        {
            continue;
        }

        const auto relative = RelativeToSourceRoot(entry.path()).generic_string();
        const auto lines = ReadLines(entry.path());
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            const auto& line = lines[i];
            if (line.find("#include") == std::string::npos)
            {
                continue;
            }

            const bool includesRmlUi =
                line.find("<RmlUi/") != std::string::npos ||
                line.find("\"RmlUi/") != std::string::npos;
            if (!includesRmlUi || StartsWith(relative, allowedPrefix))
            {
                continue;
            }

            offenders.push_back(
                relative + ":" + std::to_string(i + 1) + ": " + line);
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "RmlUi headers must stay isolated to Backends/src/UI/RmlUi. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());
}
