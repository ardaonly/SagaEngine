/// @file ChaosProfile.cpp
/// @brief Implements SagaChaosLab chaos profile loading and validation.

#include "SagaChaosLab/ChaosProfile.hpp"

#include "SagaStressArena/StressScenarioConfig.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <string_view>
#include <utility>

namespace SagaChaosLab
{
namespace
{

using Json = nlohmann::json;

constexpr std::uint32_t kMaxChaosDeferTicks = 1024;

const std::array<std::string_view, 11> kTopLevelFields = {
    "schemaVersion",
    "profileName",
    "scenario",
    "tier",
    "manualOnly",
    "seed",
    "actors",
    "ticks",
    "maxDurationSec",
    "reportDirectory",
    "chaos"};

const std::array<std::string_view, 5> kChaosFields = {
    "dropPermille",
    "duplicatePermille",
    "deferPermille",
    "deferTicks",
    "maxDeferredFrames"};

void AddDiagnostic(std::vector<std::string>& diagnostics, std::string message)
{
    diagnostics.push_back(std::move(message));
}

[[nodiscard]] bool ContainsField(
    const std::array<std::string_view, 11>& fields,
    std::string_view name)
{
    return std::find(fields.begin(), fields.end(), name) != fields.end();
}

[[nodiscard]] bool ContainsChaosField(std::string_view name)
{
    return std::find(kChaosFields.begin(), kChaosFields.end(), name) !=
           kChaosFields.end();
}

void RequireField(const Json& object,
                  std::string_view field,
                  std::vector<std::string>& diagnostics)
{
    if (!object.contains(field))
    {
        AddDiagnostic(
            diagnostics,
            "Missing required field: " + std::string(field) + ".");
    }
}

[[nodiscard]] bool ReadString(const Json& object,
                              std::string_view field,
                              std::string& outValue,
                              std::vector<std::string>& diagnostics)
{
    if (!object.contains(field))
    {
        return false;
    }
    if (!object.at(field).is_string())
    {
        AddDiagnostic(
            diagnostics,
            "Field must be a string: " + std::string(field) + ".");
        return false;
    }
    outValue = object.at(field).get<std::string>();
    return true;
}

[[nodiscard]] bool ReadBool(const Json& object,
                            std::string_view field,
                            bool& outValue,
                            std::vector<std::string>& diagnostics)
{
    if (!object.contains(field))
    {
        return false;
    }
    if (!object.at(field).is_boolean())
    {
        AddDiagnostic(
            diagnostics,
            "Field must be a bool: " + std::string(field) + ".");
        return false;
    }
    outValue = object.at(field).get<bool>();
    return true;
}

[[nodiscard]] bool ReadUint32(const Json& object,
                              std::string_view field,
                              std::uint32_t& outValue,
                              std::vector<std::string>& diagnostics)
{
    if (!object.contains(field))
    {
        return false;
    }
    if (!object.at(field).is_number_unsigned())
    {
        AddDiagnostic(
            diagnostics,
            "Field must be an unsigned integer: " + std::string(field) + ".");
        return false;
    }

    const auto value = object.at(field).get<std::uint64_t>();
    if (value > static_cast<std::uint64_t>(
                    std::numeric_limits<std::uint32_t>::max()))
    {
        AddDiagnostic(
            diagnostics,
            "Field exceeds uint32 range: " + std::string(field) + ".");
        return false;
    }

    outValue = static_cast<std::uint32_t>(value);
    return true;
}

[[nodiscard]] bool ReadUint64(const Json& object,
                              std::string_view field,
                              std::uint64_t& outValue,
                              std::vector<std::string>& diagnostics)
{
    if (!object.contains(field))
    {
        return false;
    }
    if (!object.at(field).is_number_unsigned())
    {
        AddDiagnostic(
            diagnostics,
            "Field must be an unsigned integer: " + std::string(field) + ".");
        return false;
    }
    outValue = object.at(field).get<std::uint64_t>();
    return true;
}

void ValidateUnknownFields(const Json& object,
                           std::vector<std::string>& diagnostics)
{
    for (auto it = object.begin(); it != object.end(); ++it)
    {
        if (!ContainsField(kTopLevelFields, it.key()))
        {
            AddDiagnostic(
                diagnostics,
                "Unsupported profile field: " + it.key() + ".");
        }
    }
}

void ValidateUnknownChaosFields(const Json& object,
                                std::vector<std::string>& diagnostics)
{
    for (auto it = object.begin(); it != object.end(); ++it)
    {
        if (!ContainsChaosField(it.key()))
        {
            AddDiagnostic(
                diagnostics,
                "Unsupported chaos field: " + it.key() + ".");
        }
    }
}

[[nodiscard]] ChaosProfileResult ParseProfile(const Json& root)
{
    ChaosProfileResult result;
    auto& profile = result.profile;
    auto& diagnostics = result.diagnostics;

    if (!root.is_object())
    {
        AddDiagnostic(diagnostics, "Chaos profile root must be an object.");
        return result;
    }

    ValidateUnknownFields(root, diagnostics);
    for (const auto field : kTopLevelFields)
    {
        RequireField(root, field, diagnostics);
    }

    (void)ReadUint32(root, "schemaVersion", profile.schemaVersion, diagnostics);
    (void)ReadString(root, "profileName", profile.profileName, diagnostics);
    (void)ReadString(root, "scenario", profile.scenario, diagnostics);
    (void)ReadBool(root, "manualOnly", profile.manualOnly, diagnostics);
    (void)ReadUint64(root, "seed", profile.seed, diagnostics);
    (void)ReadUint32(root, "actors", profile.actors, diagnostics);
    (void)ReadUint32(root, "ticks", profile.ticks, diagnostics);
    (void)ReadUint32(
        root,
        "maxDurationSec",
        profile.maxDurationSec,
        diagnostics);

    std::string tierText;
    if (ReadString(root, "tier", tierText, diagnostics))
    {
        const auto parsedTier = SagaStressArena::ParseStressTier(tierText);
        if (parsedTier.has_value())
        {
            profile.tier = *parsedTier;
        }
        else
        {
            AddDiagnostic(
                diagnostics,
                "tier must be smoke, mini_soak, or heavy.");
        }
    }

    std::string reportDirectory;
    if (ReadString(root, "reportDirectory", reportDirectory, diagnostics))
    {
        profile.reportDirectory = reportDirectory;
    }

    if (root.contains("chaos"))
    {
        if (!root.at("chaos").is_object())
        {
            AddDiagnostic(diagnostics, "chaos must be an object.");
        }
        else
        {
            const auto& chaos = root.at("chaos");
            ValidateUnknownChaosFields(chaos, diagnostics);
            for (const auto field : kChaosFields)
            {
                RequireField(chaos, field, diagnostics);
            }

            profile.chaosConfig.enabled = true;
            profile.chaosConfig.seed = profile.seed;
            (void)ReadUint32(
                chaos,
                "dropPermille",
                profile.chaosConfig.dropPermille,
                diagnostics);
            (void)ReadUint32(
                chaos,
                "duplicatePermille",
                profile.chaosConfig.duplicatePermille,
                diagnostics);
            (void)ReadUint32(
                chaos,
                "deferPermille",
                profile.chaosConfig.deferPermille,
                diagnostics);
            (void)ReadUint32(
                chaos,
                "deferTicks",
                profile.chaosConfig.deferTicks,
                diagnostics);

            std::uint32_t maxDeferredFrames = 0;
            if (ReadUint32(
                    chaos,
                    "maxDeferredFrames",
                    maxDeferredFrames,
                    diagnostics))
            {
                profile.chaosConfig.maxDeferredFrames = maxDeferredFrames;
            }
        }
    }

    const auto validation = ValidateChaosProfile(profile);
    diagnostics.insert(
        diagnostics.end(),
        validation.diagnostics.begin(),
        validation.diagnostics.end());
    return result;
}

} // namespace

ChaosProfile DefaultSmokeChaosProfile()
{
    ChaosProfile profile;
    profile.profileName = "default_smoke";
    profile.scenario = SagaStressArena::kDirectZonePacketChaosSmokeScenario;
    profile.tier = SagaStressArena::StressTier::Smoke;
    profile.manualOnly = false;
    profile.seed = 0x5A6A2026u;
    profile.actors = 3;
    profile.ticks = 10;
    profile.maxDurationSec = 10;
    profile.reportDirectory = "diagnostics/reports/saga_chaos_lab";
    profile.chaosConfig.enabled = true;
    profile.chaosConfig.seed = profile.seed;
    profile.chaosConfig.dropPermille = 250;
    profile.chaosConfig.duplicatePermille = 250;
    profile.chaosConfig.deferPermille = 250;
    profile.chaosConfig.deferTicks = 1;
    profile.chaosConfig.maxDeferredFrames = 16;
    return profile;
}

ChaosProfileResult LoadChaosProfile(const std::filesystem::path& profilePath)
{
    ChaosProfileResult result;
    std::ifstream input(profilePath);
    if (!input.is_open())
    {
        AddDiagnostic(
            result.diagnostics,
            "Chaos profile file could not be opened.");
        return result;
    }

    Json root;
    try
    {
        input >> root;
    }
    catch (const nlohmann::json::exception&)
    {
        AddDiagnostic(
            result.diagnostics,
            "Chaos profile JSON could not be parsed.");
        return result;
    }

    return ParseProfile(root);
}

ChaosProfileResult ValidateChaosProfile(const ChaosProfile& profile)
{
    ChaosProfileResult result;
    result.profile = profile;
    auto& diagnostics = result.diagnostics;

    if (profile.schemaVersion != kChaosProfileSchemaVersion)
    {
        AddDiagnostic(diagnostics, "schemaVersion must be 1.");
    }
    if (profile.profileName.empty())
    {
        AddDiagnostic(diagnostics, "profileName must be non-empty.");
    }
    if (profile.scenario != SagaStressArena::kDirectZonePacketChaosSmokeScenario)
    {
        AddDiagnostic(
            diagnostics,
            "scenario must be direct_zone_packet_chaos_smoke.");
    }
    if (profile.reportDirectory.empty())
    {
        AddDiagnostic(diagnostics, "reportDirectory must be non-empty.");
    }

    const auto tierDefinition = SagaStressArena::DefinitionForTier(profile.tier);
    if (profile.actors == 0 || profile.actors > tierDefinition.maxActors)
    {
        AddDiagnostic(
            diagnostics,
            "actors must be nonzero and within tier bounds.");
    }
    if (profile.ticks == 0 || profile.ticks > tierDefinition.maxTicks)
    {
        AddDiagnostic(
            diagnostics,
            "ticks must be nonzero and within tier bounds.");
    }
    if (profile.maxDurationSec == 0 ||
        profile.maxDurationSec > tierDefinition.maxDurationSec)
    {
        AddDiagnostic(
            diagnostics,
            "maxDurationSec must be nonzero and within tier bounds.");
    }

    if (profile.chaosConfig.dropPermille > 1000 ||
        profile.chaosConfig.duplicatePermille > 1000 ||
        profile.chaosConfig.deferPermille > 1000)
    {
        AddDiagnostic(
            diagnostics,
            "chaos probability fields must be between 0 and 1000.");
    }

    const std::uint64_t probabilitySum =
        static_cast<std::uint64_t>(profile.chaosConfig.dropPermille) +
        static_cast<std::uint64_t>(profile.chaosConfig.duplicatePermille) +
        static_cast<std::uint64_t>(profile.chaosConfig.deferPermille);
    if (probabilitySum > 1000)
    {
        AddDiagnostic(
            diagnostics,
            "chaos probability sum must be <= 1000.");
    }

    if (profile.chaosConfig.deferTicks == 0 ||
        profile.chaosConfig.deferTicks > kMaxChaosDeferTicks ||
        profile.chaosConfig.deferTicks > tierDefinition.maxTicks)
    {
        AddDiagnostic(
            diagnostics,
            "chaos.deferTicks must be nonzero and within tier bounds.");
    }

    const std::uint32_t maxDeferredFrames =
        static_cast<std::uint32_t>(profile.chaosConfig.maxDeferredFrames);
    if (profile.chaosConfig.maxDeferredFrames == 0 ||
        profile.chaosConfig.maxDeferredFrames >
            static_cast<std::size_t>(tierDefinition.maxActors) * 4u ||
        static_cast<std::size_t>(maxDeferredFrames) !=
            profile.chaosConfig.maxDeferredFrames)
    {
        AddDiagnostic(
            diagnostics,
            "chaos.maxDeferredFrames must be nonzero and within tier bounds.");
    }

    return result;
}

} // namespace SagaChaosLab
