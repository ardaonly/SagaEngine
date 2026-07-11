/// @file StarterArenaPlayableTests.cpp
/// @brief GPU-free contract tests for the visible StarterArena frame.

#include "StarterArenaPlayableScene.h"
#include "StarterArenaInput.h"
#include "StarterArenaSimulation.h"

#include <gtest/gtest.h>

#include <SagaEngine/Render/Materials/Material.h>
#include <SagaEngine/Render/Materials/MeshAsset.h>
#include <SagaEngine/Input/Frames/RawInputFrame.h>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <unordered_set>

namespace
{

namespace App = SagaRuntimeApp;
namespace Backend = SagaEngine::Render::Backend;
namespace RenderWorld = SagaEngine::Render::World;
namespace Input = SagaEngine::Input;

App::StarterArenaScene MakeScene()
{
    App::StarterArenaScene scene;
    scene.sceneId = "starter-arena-local-loop";
    scene.playerSpawn = {0.0, 0.0};
    scene.bounds = {-1.0, 1.0, -1.0, 1.0};
    scene.camera = {"fixed", 0.0, 0.0, 2.0, 2.0};
    scene.testInput = {4.0, 2.0};
    scene.expected = {1.0, 0.96, 15};
    return scene;
}

class TrackingBackend final : public Backend::IRenderBackend
{
public:
    bool Initialize(const Backend::SwapchainDesc&) override
    {
        initialized = true;
        return true;
    }
    void Shutdown() override { initialized = false; }
    void OnResize(std::uint32_t, std::uint32_t) override {}

    RenderWorld::MeshId CreateMesh(const SagaEngine::Render::MeshAsset&) override
    {
        if (failMeshCreation)
            return RenderWorld::MeshId::kInvalid;
        const auto id = static_cast<RenderWorld::MeshId>(nextMesh++);
        meshes.insert(static_cast<std::uint32_t>(id));
        return id;
    }
    RenderWorld::MaterialId CreateMaterial(const SagaEngine::Render::MaterialRuntime&) override
    {
        if (failMaterialCreation)
            return RenderWorld::MaterialId::kInvalid;
        const auto id = static_cast<RenderWorld::MaterialId>(nextMaterial++);
        materials.insert(static_cast<std::uint32_t>(id));
        return id;
    }
    void DestroyMesh(RenderWorld::MeshId id) override
    {
        meshes.erase(static_cast<std::uint32_t>(id));
    }
    void DestroyMaterial(RenderWorld::MaterialId id) override
    {
        materials.erase(static_cast<std::uint32_t>(id));
    }
    SagaEngine::Render::TextureHandle CreateTexture(std::uint32_t,
                                                    std::uint32_t,
                                                    const std::uint8_t*) override
    {
        if (failTextureCreation)
            return SagaEngine::Render::TextureHandle::kInvalid;
        const auto id = static_cast<SagaEngine::Render::TextureHandle>(nextTexture++);
        textures.insert(static_cast<std::uint32_t>(id));
        return id;
    }
    void DestroyTexture(SagaEngine::Render::TextureHandle id) override
    {
        textures.erase(static_cast<std::uint32_t>(id));
    }
    void BeginFrame() override {}
    void Submit(const SagaEngine::Render::Scene::Camera&,
                const SagaEngine::Render::Scene::RenderView& view) override
    {
        submittedDraws = view.DrawCount();
    }
    void EndFrame() override {}

    bool failMeshCreation = false;
    bool failMaterialCreation = false;
    bool failTextureCreation = false;
    bool initialized = false;
    std::size_t submittedDraws = 0;
    std::unordered_set<std::uint32_t> meshes;
    std::unordered_set<std::uint32_t> materials;
    std::unordered_set<std::uint32_t> textures;

private:
    std::uint32_t nextMesh = 1;
    std::uint32_t nextMaterial = 1;
    std::uint32_t nextTexture = 1;
};

class FakeInputBackend final : public SagaEngine::Platform::IPlatformInputBackend
{
public:
    explicit FakeInputBackend(std::vector<Input::RawInputFrame> frames)
        : frames_(std::move(frames))
    {
    }

    bool Initialize() override { return true; }
    void Shutdown() override { shutdown = true; }
    void PollRawEvents(Input::RawInputFrame& output) override
    {
        output.Clear();
        if (index_ < frames_.size())
        {
            output = frames_[index_++];
        }
    }
    void SetTextInputEnabled(bool) override {}
    std::string_view GetBackendName() const noexcept override { return "FakeStarterArena"; }

    bool shutdown = false;

private:
    std::vector<Input::RawInputFrame> frames_;
    std::size_t index_ = 0;
};

std::filesystem::path WriteSyntheticScript(std::string_view name, std::string_view json)
{
    const auto path = std::filesystem::temp_directory_path() / std::string(name);
    std::ofstream output(path);
    output << json;
    return path;
}

App::StarterArenaInputFrame InputFrame(double x, double y, std::uint32_t tick = 0)
{
    App::StarterArenaInputFrame frame;
    frame.tick = tick;
    frame.movementVelocity = {x, y};
    return frame;
}

} // namespace

TEST(StarterArenaPlayableTests, SharedSimulationMatchesHeadlessExpectation)
{
    const auto scene = MakeScene();
    auto simulation = App::MakeStarterArenaSimulation(scene);
    auto provider = App::CreateSceneInputProvider(scene.testInput);
    for (std::uint32_t frame = 0; frame < 30u; ++frame)
    {
        App::StepStarterArenaSimulation(simulation, provider->Read(frame), 0.016);
    }
    EXPECT_EQ(simulation.ticks, 30u);
    EXPECT_EQ(simulation.clampCount, 15u);
    EXPECT_DOUBLE_EQ(simulation.position.x, 1.0);
    EXPECT_NEAR(simulation.position.y, 0.96, 0.000001);
}

TEST(StarterArenaPlayableTests, AppLocalInputDoesNotReferenceForbiddenRuntimePaths)
{
    const auto sourceRoot = std::filesystem::path(SAGA_SOURCE_ROOT);
    for (const auto relative : {"Apps/Runtime/StarterArenaInput.h",
                                "Apps/Runtime/StarterArenaInput.cpp"})
    {
        std::ifstream input(sourceRoot / relative);
        ASSERT_TRUE(input) << relative;
        const std::string text((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
        for (const auto forbidden : {"ClientHost", "Apps/Sandbox", "Apps/Server",
                                     "InputCommand", "replication", "prediction"})
        {
            EXPECT_EQ(text.find(forbidden), std::string::npos)
                << relative << " references " << forbidden;
        }
    }
}

TEST(StarterArenaPlayableTests, SyntheticInputMovesDeterministically)
{
    const auto path = WriteSyntheticScript(
        "starter_arena_synthetic_valid.json",
        R"({"schemaVersion":1,"sourceKind":"StarterArenaSyntheticInput","segments":[{"startTick":0,"endTickExclusive":15,"actions":["MoveRight"]},{"startTick":15,"endTickExclusive":30,"actions":["MoveUp"]}]})");
    std::string error;
    auto provider = App::CreateSyntheticInputProvider(path, error);
    ASSERT_NE(provider, nullptr) << error;
    auto simulation = App::MakeStarterArenaSimulation(MakeScene());
    App::StarterArenaInputEvidence evidence;
    evidence.source = App::StarterArenaInputSource::Synthetic;
    for (std::uint32_t tick = 0; tick < 30; ++tick)
    {
        const auto input = provider->Read(tick);
        App::RecordStarterArenaInput(input, evidence);
        App::StepStarterArenaSimulation(simulation, input, 0.016);
    }
    App::FinalizeStarterArenaInputEvidence(evidence);

    EXPECT_NEAR(simulation.position.x, 0.48, 0.000001);
    EXPECT_NEAR(simulation.position.y, 0.48, 0.000001);
    EXPECT_EQ(simulation.clampCount, 0u);
    EXPECT_EQ(evidence.moveRightCount, 15u);
    EXPECT_EQ(evidence.moveUpCount, 15u);
    EXPECT_EQ(evidence.framesWithInput, 30u);
    EXPECT_EQ(evidence.status, "Passed");
}

TEST(StarterArenaPlayableTests, OpposingActionsCancelAndDiagonalsNormalize)
{
    const auto path = WriteSyntheticScript(
        "starter_arena_synthetic_actions.json",
        R"({"schemaVersion":1,"sourceKind":"StarterArenaSyntheticInput","segments":[{"startTick":0,"endTickExclusive":1,"actions":["MoveLeft","MoveRight"]},{"startTick":1,"endTickExclusive":2,"actions":["MoveRight","MoveUp"]}]})");
    std::string error;
    auto provider = App::CreateSyntheticInputProvider(path, error);
    ASSERT_NE(provider, nullptr) << error;
    const auto cancelled = provider->Read(0);
    EXPECT_DOUBLE_EQ(cancelled.movementVelocity.x, 0.0);
    EXPECT_DOUBLE_EQ(cancelled.movementVelocity.y, 0.0);
    const auto diagonal = provider->Read(1);
    EXPECT_NEAR(std::hypot(diagonal.movementVelocity.x, diagonal.movementVelocity.y),
                2.0,
                0.000001);
}

TEST(StarterArenaPlayableTests, SimulationClampsInputToBounds)
{
    auto simulation = App::MakeStarterArenaSimulation(MakeScene());
    App::StepStarterArenaSimulation(simulation, InputFrame(100.0, -100.0), 1.0);
    EXPECT_DOUBLE_EQ(simulation.position.x, 1.0);
    EXPECT_DOUBLE_EQ(simulation.position.y, -1.0);
    EXPECT_EQ(simulation.clampCount, 1u);
}

TEST(StarterArenaPlayableTests, EmptySyntheticInputKeepsPlayerAtSpawn)
{
    const auto path = WriteSyntheticScript(
        "starter_arena_synthetic_empty.json",
        R"({"schemaVersion":1,"sourceKind":"StarterArenaSyntheticInput","segments":[]})");
    std::string error;
    auto provider = App::CreateSyntheticInputProvider(path, error);
    ASSERT_NE(provider, nullptr) << error;
    auto simulation = App::MakeStarterArenaSimulation(MakeScene());
    for (std::uint32_t tick = 0; tick < 30; ++tick)
    {
        App::StepStarterArenaSimulation(simulation, provider->Read(tick), 0.016);
    }
    EXPECT_DOUBLE_EQ(simulation.position.x, 0.0);
    EXPECT_DOUBLE_EQ(simulation.position.y, 0.0);
}

TEST(StarterArenaPlayableTests, FakeKeyboardBackendUsesInputManagerActionMapPath)
{
    Input::RawInputFrame press;
    press.keys.push_back(
        {Input::KeyCode::D, true, false, Input::ModifierFlags::None, 0});
    auto backend = std::make_unique<FakeInputBackend>(
        std::vector<Input::RawInputFrame>{press, {}});
    std::string error;
    auto provider = App::CreateKeyboardInputProvider(std::move(backend), error);
    ASSERT_NE(provider, nullptr) << error;

    App::StarterArenaInputEvidence evidence;
    evidence.source = App::StarterArenaInputSource::Keyboard;
    const auto first = provider->Read(0);
    const auto held = provider->Read(1);
    App::RecordStarterArenaInput(first, evidence);
    App::RecordStarterArenaInput(held, evidence);
    App::FinalizeStarterArenaInputEvidence(evidence);
    provider->Shutdown();

    EXPECT_TRUE(first.actions.moveRight);
    EXPECT_TRUE(held.actions.moveRight);
    EXPECT_DOUBLE_EQ(first.movementVelocity.x, 2.0);
    EXPECT_EQ(evidence.moveRightCount, 2u);
    EXPECT_EQ(evidence.framesWithInput, 2u);
    EXPECT_TRUE(evidence.realDeviceObserved);
    EXPECT_EQ(evidence.status, "Passed");
}

TEST(StarterArenaPlayableTests, IdleKeyboardEvidenceDoesNotFailTheRun)
{
    auto backend = std::make_unique<FakeInputBackend>(
        std::vector<Input::RawInputFrame>{Input::RawInputFrame{}});
    std::string error;
    auto provider = App::CreateKeyboardInputProvider(std::move(backend), error);
    ASSERT_NE(provider, nullptr) << error;
    App::StarterArenaInputEvidence evidence;
    evidence.source = App::StarterArenaInputSource::Keyboard;
    App::RecordStarterArenaInput(provider->Read(0), evidence);
    App::FinalizeStarterArenaInputEvidence(evidence);
    provider->Shutdown();
    EXPECT_EQ(evidence.status, "NoInputObserved");
    EXPECT_FALSE(evidence.realDeviceObserved);
    EXPECT_EQ(evidence.framesWithInput, 0u);
}

TEST(StarterArenaPlayableTests, SyntheticParserRejectsMalformedContracts)
{
    const std::vector<std::string> invalid = {
        R"({"schemaVersion":2,"sourceKind":"StarterArenaSyntheticInput","segments":[]})",
        R"({"schemaVersion":"1","sourceKind":42,"segments":[]})",
        R"({"schemaVersion":1,"sourceKind":"StarterArenaSyntheticInput","segments":[{"startTick":2,"endTickExclusive":2,"actions":[]}]})",
        R"({"schemaVersion":1,"sourceKind":"StarterArenaSyntheticInput","segments":[{"startTick":0,"endTickExclusive":2,"actions":["Jump"]}]})",
        R"({"schemaVersion":1,"sourceKind":"StarterArenaSyntheticInput","segments":[{"startTick":0,"endTickExclusive":3,"actions":[]},{"startTick":2,"endTickExclusive":4,"actions":[]}]})"};
    for (std::size_t index = 0; index < invalid.size(); ++index)
    {
        std::string error;
        const auto path = WriteSyntheticScript(
            "starter_arena_synthetic_invalid_" + std::to_string(index) + ".json",
            invalid[index]);
        EXPECT_EQ(App::CreateSyntheticInputProvider(path, error), nullptr);
        EXPECT_FALSE(error.empty());
    }
}

TEST(StarterArenaPlayableTests, SceneBuildsCameraArenaAndPlayerDraw)
{
    TrackingBackend backend;
    RenderWorld::RenderWorld world;
    const auto sceneSource = MakeScene();
    auto scene = App::CreateStarterArenaPlayableScene(backend, world, sceneSource, 1280u, 720u);

    ASSERT_TRUE(scene.IsValid());
    EXPECT_EQ(world.LiveCount(), 6u);
    EXPECT_FLOAT_EQ(scene.camera.position.x, 0.0f);
    EXPECT_FLOAT_EQ(scene.camera.position.y, 5.0f);
    EXPECT_FLOAT_EQ(scene.camera.position.z, 0.0f);

    auto view = App::BuildStarterArenaPlayableView(world, scene.camera);
    EXPECT_EQ(view.visited, 6u);
    EXPECT_GE(view.DrawCount(), 2u);
    EXPECT_TRUE(App::ViewContainsEntity(view, scene.player));

    backend.BeginFrame();
    backend.Submit(scene.camera, view);
    backend.EndFrame();
    EXPECT_EQ(backend.submittedDraws, view.DrawCount());

    App::DestroyStarterArenaPlayableScene(backend, world, scene);
    EXPECT_EQ(world.LiveCount(), 0u);
    EXPECT_TRUE(backend.meshes.empty());
    EXPECT_TRUE(backend.materials.empty());
    EXPECT_TRUE(backend.textures.empty());
}

TEST(StarterArenaPlayableTests, PlayerTransformUsesSimulationCoordinates)
{
    TrackingBackend backend;
    RenderWorld::RenderWorld world;
    auto scene = App::CreateStarterArenaPlayableScene(backend, world, MakeScene(), 800u, 800u);
    ASSERT_TRUE(scene.IsValid());

    App::UpdateStarterArenaPlayerTransform(world, scene.player, {0.75, -0.5});
    const auto* player = world.Get(scene.player);
    ASSERT_NE(player, nullptr);
    EXPECT_FLOAT_EQ(player->transform.position.x, 0.75f);
    EXPECT_FLOAT_EQ(player->transform.position.y, 0.4f);
    EXPECT_FLOAT_EQ(player->transform.position.z, -0.5f);

    App::DestroyStarterArenaPlayableScene(backend, world, scene);
}

TEST(StarterArenaPlayableTests, PartialResourceFailureCleansAcceptedResources)
{
    TrackingBackend backend;
    backend.failMaterialCreation = true;
    RenderWorld::RenderWorld world;
    auto scene = App::CreateStarterArenaPlayableScene(backend, world, MakeScene(), 1280u, 720u);

    EXPECT_FALSE(scene.IsValid());
    EXPECT_EQ(world.LiveCount(), 0u);
    EXPECT_TRUE(backend.meshes.empty());
    EXPECT_TRUE(backend.materials.empty());
    EXPECT_TRUE(backend.textures.empty());
}
