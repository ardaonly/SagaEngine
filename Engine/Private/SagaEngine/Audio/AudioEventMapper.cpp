/// @file AudioEventMapper.cpp
/// @brief Gameplay intent → AudioEvent mapping with context enrichment.

#include "SagaEngine/Audio/AudioEventMapper.h"

#include <cmath>
#include <functional>

namespace SagaEngine::Audio
{

// ── Helpers ──────────────────────────────────────────────────────────

static float Distance(const Math::Vec3& a, const Math::Vec3& b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

static DistanceBand ClassifyDistance(float d)
{
    if (d < 5.f)  return DistanceBand::Near;
    if (d < 30.f) return DistanceBand::Mid;
    if (d < 80.f) return DistanceBand::Far;
    return DistanceBand::VeryFar;
}

static std::uint64_t MakeDedupKey(AudioCategory cat, AudioObjectId obj)
{
    return (static_cast<std::uint64_t>(cat) << 56) |
            static_cast<std::uint64_t>(obj);
}

// ── Registration ─────────────────────────────────────────────────────

void AudioEventMapper::Register(const std::string& intentType, MappingFn fn)
{
    m_rules[intentType].push_back(std::move(fn));
}

// ── Mapping ──────────────────────────────────────────────────────────

void AudioEventMapper::Map(const GameplayAudioIntent& intent,
                           const ListenerContext& ctx,
                           AudioEventBatch& out) const
{
    auto it = m_rules.find(intent.type);
    if (it == m_rules.end()) return;

    for (auto& fn : it->second)
        fn(intent, ctx, out);
}

void AudioEventMapper::MapAll(const std::vector<GameplayAudioIntent>& intents,
                              const ListenerContext& ctx,
                              AudioEventBatch& out) const
{
    for (auto& intent : intents)
        Map(intent, ctx, out);
}

// ── Default rules ────────────────────────────────────────────────────

void AudioEventMapper::RegisterDefaults()
{
    // ─ WeaponFired ───────────────────────────────────────────────
    Register("WeaponFired", [](const GameplayAudioIntent& intent,
                               const ListenerContext& ctx,
                               AudioEventBatch& out)
    {
        float dist = Distance(intent.sourceTransform.position,
                              ctx.listenerTransform.position);
        DistanceBand band = ClassifyDistance(dist);

        AudioEvent e;
        // 1P vs 3P decision
        if (intent.sourceObject == AudioObjectId::kInvalid)
            return; // no source — skip

        bool isLocal = ctx.isFirstPerson && (dist < 1.f);
        e.eventName = isLocal ? "Play_Weapon_Fire_1P" : "Play_Weapon_Fire_3P";

        e.gameObject   = intent.sourceObject;
        e.category     = AudioCategory::Weapon;
        e.priority     = AudioPriority::High;
        e.transform    = intent.sourceTransform;
        e.distanceBand = band;
        e.distance     = dist;
        e.dedupKey     = MakeDedupKey(AudioCategory::Weapon, intent.sourceObject);

        // RTPCs
        e.rtpcs.push_back({"Distance", dist});
        e.rtpcs.push_back({"Intensity", intent.intensity});

        // Switches
        e.switches.push_back({"WeaponId", std::to_string(intent.weaponId)});

        out.Push(std::move(e));
    });

    // ─ Footstep ──────────────────────────────────────────────────
    Register("Footstep", [](const GameplayAudioIntent& intent,
                            const ListenerContext& ctx,
                            AudioEventBatch& out)
    {
        float dist = Distance(intent.sourceTransform.position,
                              ctx.listenerTransform.position);
        DistanceBand band = ClassifyDistance(dist);

        // Footsteps beyond Mid range are usually inaudible — cull early.
        if (band == DistanceBand::Far || band == DistanceBand::VeryFar)
            return;

        AudioEvent e;
        e.eventName    = "Play_Footstep";
        e.gameObject   = intent.sourceObject;
        e.category     = AudioCategory::Footstep;
        e.priority     = AudioPriority::Low;
        e.transform    = intent.sourceTransform;
        e.distanceBand = band;
        e.distance     = dist;
        e.dedupKey     = MakeDedupKey(AudioCategory::Footstep, intent.sourceObject);

        // Surface switch
        const char* surfaceName = "Default";
        switch (intent.surfaceType)
        {
            case SurfaceType::Stone: surfaceName = "Stone"; break;
            case SurfaceType::Wood:  surfaceName = "Wood";  break;
            case SurfaceType::Metal: surfaceName = "Metal"; break;
            case SurfaceType::Dirt:  surfaceName = "Dirt";  break;
            case SurfaceType::Grass: surfaceName = "Grass"; break;
            case SurfaceType::Water: surfaceName = "Water"; break;
            case SurfaceType::Snow:  surfaceName = "Snow";  break;
            case SurfaceType::Sand:  surfaceName = "Sand";  break;
            default: break;
        }
        e.switches.push_back({"SurfaceType", surfaceName});
        e.rtpcs.push_back({"Distance", dist});

        out.Push(std::move(e));
    });

    // ─ SpellCast ─────────────────────────────────────────────────
    Register("SpellCastStarted", [](const GameplayAudioIntent& intent,
                                    const ListenerContext& ctx,
                                    AudioEventBatch& out)
    {
        float dist = Distance(intent.sourceTransform.position,
                              ctx.listenerTransform.position);

        AudioEvent e;
        e.eventName    = "Play_Spell_Cast";
        e.gameObject   = intent.sourceObject;
        e.category     = AudioCategory::Spell;
        e.priority     = AudioPriority::Normal;
        e.transform    = intent.sourceTransform;
        e.distanceBand = ClassifyDistance(dist);
        e.distance     = dist;
        e.dedupKey     = MakeDedupKey(AudioCategory::Spell, intent.sourceObject);

        e.switches.push_back({"SpellId", std::to_string(intent.spellId)});
        e.rtpcs.push_back({"Distance", dist});
        e.rtpcs.push_back({"Intensity", intent.intensity});

        out.Push(std::move(e));
    });

    // ─ EntityDied ────────────────────────────────────────────────
    Register("EntityDied", [](const GameplayAudioIntent& intent,
                              const ListenerContext& ctx,
                              AudioEventBatch& out)
    {
        float dist = Distance(intent.sourceTransform.position,
                              ctx.listenerTransform.position);

        AudioEvent e;
        e.eventName    = "Play_Death";
        e.gameObject   = intent.sourceObject;
        e.category     = AudioCategory::SFX;
        e.priority     = AudioPriority::High;
        e.transform    = intent.sourceTransform;
        e.distanceBand = ClassifyDistance(dist);
        e.distance     = dist;
        e.dedupKey     = MakeDedupKey(AudioCategory::SFX, intent.sourceObject);

        e.rtpcs.push_back({"Distance", dist});

        out.Push(std::move(e));
    });

    // ─ Impact ────────────────────────────────────────────────────
    Register("Impact", [](const GameplayAudioIntent& intent,
                          const ListenerContext& ctx,
                          AudioEventBatch& out)
    {
        float dist = Distance(intent.sourceTransform.position,
                              ctx.listenerTransform.position);

        AudioEvent e;
        e.eventName    = "Play_Impact";
        e.gameObject   = intent.sourceObject;
        e.category     = AudioCategory::Impact;
        e.priority     = AudioPriority::Normal;
        e.transform    = intent.sourceTransform;
        e.distanceBand = ClassifyDistance(dist);
        e.distance     = dist;
        e.dedupKey     = MakeDedupKey(AudioCategory::Impact, intent.sourceObject);

        const char* surfaceName = "Default";
        switch (intent.surfaceType)
        {
            case SurfaceType::Stone: surfaceName = "Stone"; break;
            case SurfaceType::Wood:  surfaceName = "Wood";  break;
            case SurfaceType::Metal: surfaceName = "Metal"; break;
            default: break;
        }
        e.switches.push_back({"SurfaceType", surfaceName});
        e.rtpcs.push_back({"Distance", dist});
        e.rtpcs.push_back({"Intensity", intent.intensity});

        out.Push(std::move(e));
    });
}

} // namespace SagaEngine::Audio
