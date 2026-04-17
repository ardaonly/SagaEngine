/// @file ECSProbeScenario.cpp
/// @brief ECS probe scenario — entity/component/query stress.
///
/// API contract fixes applied (2026-04-07):
///   - ComponentRegistry::Register<T>(id, name)  — NOT RegisterComponent
///   - IsRegistered<T>() guard → scenario re-launch safe (no duplicate throws)
///   - m_world->GetAliveEntities()              — NOT world.Query({})
///   - WorldState has no archetype manager      — replaced with component-type table
///   - Stable sandbox component IDs in [1001, 1099] range

#include "ECSProbeScenario.h"
#include "SagaSandbox/Core/ScenarioRegistry.h"

#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Core/Time/Time.h>
#include <SagaEngine/ECS/ComponentRegistry.h>
#include <SagaEngine/ECS/ComponentSerializerRegistry.h>
#include <SagaEngine/ECS/Query.h>

#include <imgui.h>
#include <chrono>
#include <cstring>
#include <random>

namespace SagaSandbox
{

SAGA_SANDBOX_REGISTER_SCENARIO(ECSProbeScenario);

static constexpr const char* kTag = "ECSProbeScenario";

// ─── Stable sandbox component IDs ────────────────────────────────────────────
//
// These IDs live in the [1001, 1099] range, reserved for Sandbox/ECSProbe.
// They MUST NOT collide with engine or server component IDs.
// If you add new sandbox components, append here and increment the ID.
//
static constexpr SagaEngine::ECS::ComponentTypeId kIdSbTransform = 1001u;
static constexpr SagaEngine::ECS::ComponentTypeId kIdSbVelocity  = 1002u;
static constexpr SagaEngine::ECS::ComponentTypeId kIdSbHealth    = 1003u;

// ─── Component registration helper ───────────────────────────────────────────
//
// Called from OnInit(). Guards against duplicate registration so the scenario
// can be stopped and restarted within the same process without throwing.
//
static void RegisterSandboxComponents()
{
    using CR  = SagaEngine::ECS::ComponentRegistry;
    using CSR = SagaEngine::ECS::ComponentSerializerRegistry;

    // ── SbTransform ───────────────────────────────────────────────────────────

    if (!CR::Instance().IsRegistered<SbTransform>())
    {
        CR::Instance().Register<SbTransform>(kIdSbTransform, "SbTransform");

        CSR::Instance().Register<SbTransform>(
            kIdSbTransform,
            "SbTransform",
            [](const SbTransform& d, void* buf, size_t sz) -> size_t {
                if (sz < sizeof(SbTransform)) return 0;
                std::memcpy(buf, &d, sizeof(SbTransform));
                return sizeof(SbTransform);
            },
            [](SbTransform& d, const void* buf, size_t sz) -> size_t {
                if (sz < sizeof(SbTransform)) return 0;
                std::memcpy(&d, buf, sizeof(SbTransform));
                return sizeof(SbTransform);
            },
            []() -> size_t { return sizeof(SbTransform); }
        );
    }

    // ── SbVelocity ────────────────────────────────────────────────────────────

    if (!CR::Instance().IsRegistered<SbVelocity>())
    {
        CR::Instance().Register<SbVelocity>(kIdSbVelocity, "SbVelocity");

        CSR::Instance().Register<SbVelocity>(
            kIdSbVelocity,
            "SbVelocity",
            [](const SbVelocity& d, void* buf, size_t sz) -> size_t {
                if (sz < sizeof(SbVelocity)) return 0;
                std::memcpy(buf, &d, sizeof(SbVelocity));
                return sizeof(SbVelocity);
            },
            [](SbVelocity& d, const void* buf, size_t sz) -> size_t {
                if (sz < sizeof(SbVelocity)) return 0;
                std::memcpy(&d, buf, sizeof(SbVelocity));
                return sizeof(SbVelocity);
            },
            []() -> size_t { return sizeof(SbVelocity); }
        );
    }

    // ── SbHealth ──────────────────────────────────────────────────────────────

    if (!CR::Instance().IsRegistered<SbHealth>())
    {
        CR::Instance().Register<SbHealth>(kIdSbHealth, "SbHealth");

        CSR::Instance().Register<SbHealth>(
            kIdSbHealth,
            "SbHealth",
            [](const SbHealth& d, void* buf, size_t sz) -> size_t {
                if (sz < sizeof(SbHealth)) return 0;
                std::memcpy(buf, &d, sizeof(SbHealth));
                return sizeof(SbHealth);
            },
            [](SbHealth& d, const void* buf, size_t sz) -> size_t {
                if (sz < sizeof(SbHealth)) return 0;
                std::memcpy(&d, buf, sizeof(SbHealth));
                return sizeof(SbHealth);
            },
            []() -> size_t { return sizeof(SbHealth); }
        );
    }
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool ECSProbeScenario::OnInit()
{
    LOG_INFO(kTag, "Initialising ECS probe…");

    m_world   = std::make_unique<SagaEngine::Simulation::WorldState>();
    m_simTick = std::make_unique<SagaEngine::Simulation::SimulationTick>();

    RegisterSandboxComponents();

    // Initial batch of entities
    SpawnBatch(50);

    LOG_INFO(kTag, "ECS probe ready (%zu entities).", m_entities.size());
    return true;
}

void ECSProbeScenario::OnUpdate(float dt, uint64_t tick)
{
    // Advance returns the number of fixed ticks ready this frame.
    // We consume them here; a real simulation would run systems per tick.
    const uint32_t ticks = m_simTick->Advance(static_cast<double>(dt));
    (void)ticks;

    if (m_autoSpawn && (tick % 30 == 0))
        SpawnBatch(m_spawnCount);
}

void ECSProbeScenario::OnShutdown()
{
    m_entities.clear();
    m_simTick.reset();
    m_world.reset();
    LOG_INFO(kTag, "ECS probe shut down. Spawned=%llu  Destroyed=%llu",
             static_cast<unsigned long long>(m_totalSpawned),
             static_cast<unsigned long long>(m_totalDestroyed));
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

void ECSProbeScenario::SpawnBatch(int count)
{
    static std::mt19937 rng{42};
    static std::uniform_real_distribution<float> dist(-100.f, 100.f);

    for (int i = 0; i < count; ++i)
    {
        const auto entity = m_world->CreateEntity();
        const auto id     = entity.id;

        m_world->AddComponent<SbTransform>(id, {dist(rng), dist(rng), dist(rng)});
        m_world->AddComponent<SbVelocity>(id,  {dist(rng) * 0.1f, dist(rng) * 0.1f, 0.f});

        // Half the entities get a Health component — exercises multi-component queries
        if (i % 2 == 0)
            m_world->AddComponent<SbHealth>(id, {100.f, 100.f});

        m_entities.push_back(id);
        ++m_totalSpawned;
    }
}

void ECSProbeScenario::DestroyRandom(int count)
{
    if (m_entities.empty()) return;

    static std::mt19937 rng{99};

    const int toDestroy = std::min(count, static_cast<int>(m_entities.size()));
    for (int i = 0; i < toDestroy; ++i)
    {
        std::uniform_int_distribution<int> pick(0, static_cast<int>(m_entities.size()) - 1);
        const int idx = pick(rng);

        m_world->DestroyEntity(m_entities[static_cast<size_t>(idx)]);
        m_entities[static_cast<size_t>(idx)] = m_entities.back();
        m_entities.pop_back();
        ++m_totalDestroyed;
    }
}

void ECSProbeScenario::RunQueryBenchmark()
{
    const auto t0 = std::chrono::steady_clock::now();

    // Query entities that have both Transform AND Velocity components.
    SagaEngine::ECS::Query q(m_world.get());
    q.WithComponent(SagaEngine::ECS::GetComponentTypeId<SbTransform>());
    q.WithComponent(SagaEngine::ECS::GetComponentTypeId<SbVelocity>());

    const auto results   = q.Execute();
    m_lastQueryHits      = static_cast<uint32_t>(results.size());

    const auto t1 = std::chrono::steady_clock::now();
    m_lastQueryMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
}

// ─── Debug UI ─────────────────────────────────────────────────────────────────

void ECSProbeScenario::OnRenderDebugUI()
{
    if (!ImGui::Begin("ECS Probe", nullptr))
    {
        ImGui::End();
        return;
    }

    // ── World overview ────────────────────────────────────────────────────────

    ImGui::SeparatorText("World State");

    // GetAliveEntities() is the correct API — WorldState has no archetype manager.
    const auto& aliveEntities = m_world->GetAliveEntities();
    ImGui::Text("Live entities    : %zu",  aliveEntities.size());
    ImGui::Text("Total spawned    : %llu", static_cast<unsigned long long>(m_totalSpawned));
    ImGui::Text("Total destroyed  : %llu", static_cast<unsigned long long>(m_totalDestroyed));
    ImGui::Text("Component types  : %u",   m_world->ComponentTypes());

    // ── Spawn / Destroy controls ──────────────────────────────────────────────

    ImGui::SeparatorText("Controls");

    ImGui::SliderInt("Spawn count",   &m_spawnCount,   1, 1000);
    ImGui::SliderInt("Destroy count", &m_destroyCount, 1, 200);
    ImGui::Checkbox("Auto-spawn every 30 ticks", &m_autoSpawn);

    if (ImGui::Button("Spawn"))
        SpawnBatch(m_spawnCount);

    ImGui::SameLine();

    if (ImGui::Button("Destroy random"))
        DestroyRandom(m_destroyCount);

    ImGui::SameLine();

    if (ImGui::Button("Destroy all"))
    {
        for (const auto id : m_entities)
            m_world->DestroyEntity(id);
        m_totalDestroyed += static_cast<uint64_t>(m_entities.size());
        m_entities.clear();
    }

    // ── Query benchmark ───────────────────────────────────────────────────────

    ImGui::SeparatorText("Query Benchmark");

    if (ImGui::Button("Run Query (Transform + Velocity)"))
        RunQueryBenchmark();

    ImGui::Text("Last hits : %u",          m_lastQueryHits);
    ImGui::Text("Query time: %.4f ms",     m_lastQueryMs);

    // ── Component-type breakdown table ────────────────────────────────────────
    //
    // WorldState uses flat per-type ComponentBlock storage — there is no
    // archetype manager. We show per-type entity counts for the 3 registered
    // sandbox types. This table grows with new component registrations naturally.
    //
    ImGui::SeparatorText("Component Breakdown");

    constexpr ImGuiTableFlags kFlags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_ScrollY;

    struct ComponentRow { const char* name; SagaEngine::ECS::ComponentTypeId id; };
    static constexpr ComponentRow kRows[] = {
        { "SbTransform", kIdSbTransform },
        { "SbVelocity",  kIdSbVelocity  },
        { "SbHealth",    kIdSbHealth    },
    };

    if (ImGui::BeginTable("##comp_breakdown", 2, kFlags, ImVec2(0, 80)))
    {
        ImGui::TableSetupColumn("Component",    ImGuiTableColumnFlags_WidthFixed,   140.f);
        ImGui::TableSetupColumn("Entity count", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const auto& row : kRows)
        {
            // Query how many alive entities have this one component type.
            const std::vector<SagaEngine::ECS::ComponentTypeId> req{ row.id };
            const auto entitiesWithType = m_world->Query(req);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.name);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%zu", entitiesWithType.size());
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace SagaSandbox