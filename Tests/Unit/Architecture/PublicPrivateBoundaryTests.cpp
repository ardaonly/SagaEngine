/// @file PublicPrivateBoundaryTests.cpp
/// @brief Guards Public and Private include boundaries for module migration.

#include <gtest/gtest.h>

#include "SagaEngine/Render/RenderPipelineConfig.h"
#include "SagaEngine/World/WorldFacade.h"

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
