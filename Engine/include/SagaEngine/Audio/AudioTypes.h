/// @file AudioTypes.h
/// @brief Core audio type definitions — handles, enums, spatial data.
///
/// Server never sends audio commands — only gameplay intents.
/// These types live on the client side and feed into the audio pipeline:
///   GameplayEvent → AudioEventMapper → AudioFilter → IAudioBackend (Wwise)

#pragma once

#include "SagaEngine/Math/Vec3.h"

#include <cstdint>
#include <string>

namespace SagaEngine::Audio
{

// ── Handles ──────────────────────────────────────────────────────────

/// Opaque handle to a registered audio game object (maps 1:1 to an entity).
enum class AudioObjectId : std::uint64_t { kInvalid = 0 };

/// Opaque handle to a posted audio event inside the backend.
enum class AudioPlayId : std::uint32_t { kInvalid = 0 };

// ── Enums ────────────────────────────────────────────────────────────

/// Category used for priority / voice-limiting decisions.
enum class AudioCategory : std::uint8_t
{
    SFX        = 0,
    Ambient    = 1,
    Music      = 2,
    Voice      = 3,
    UI         = 4,
    Footstep   = 5,
    Weapon     = 6,
    Spell      = 7,
    Impact     = 8,
};

/// Priority tier — higher numeric value = more important.
enum class AudioPriority : std::uint8_t
{
    Low    = 0,
    Normal = 1,
    High   = 2,
    Critical = 3,   ///< Never culled (player death, quest VO, etc.)
};

/// Surface type for footstep / impact variation.
enum class SurfaceType : std::uint8_t
{
    Default  = 0,
    Stone    = 1,
    Wood     = 2,
    Metal    = 3,
    Dirt     = 4,
    Grass    = 5,
    Water    = 6,
    Snow     = 7,
    Sand     = 8,
};

/// Environment tag — drives Wwise States for reverb / EQ.
enum class EnvironmentType : std::uint8_t
{
    Outdoor  = 0,
    Indoor   = 1,
    Cave     = 2,
    Forest   = 3,
    City     = 4,
    Dungeon  = 5,
    Underwater = 6,
};

/// Listener-relative distance band (computed per frame).
enum class DistanceBand : std::uint8_t
{
    Near   = 0,   ///<  0 –  5 m
    Mid    = 1,   ///<  5 – 30 m
    Far    = 2,   ///< 30 – 80 m
    VeryFar = 3,  ///< 80+  m  (candidate for culling)
};

// ── Spatial info ─────────────────────────────────────────────────────

/// 3D position + orientation for a game object or listener.
struct AudioTransform
{
    Math::Vec3 position = {0.f, 0.f, 0.f};
    Math::Vec3 forward  = {0.f, 0.f, 1.f};
    Math::Vec3 up       = {0.f, 1.f, 0.f};
};

// ── RTPC descriptor ──────────────────────────────────────────────────

/// A real-time parameter control value to pass to the backend.
struct RTPCValue
{
    std::string paramName;
    float       value = 0.f;
};

// ── Switch / State ───────────────────────────────────────────────────

struct AudioSwitch
{
    std::string groupName;
    std::string valueName;
};

struct AudioState
{
    std::string groupName;
    std::string stateName;
};

// ── Limits ───────────────────────────────────────────────────────────

constexpr std::uint32_t kMaxConcurrentVoices  = 64;
constexpr std::uint32_t kMaxAudioObjects      = 4096;
constexpr float         kDefaultCullDistance   = 100.f;

} // namespace SagaEngine::Audio
