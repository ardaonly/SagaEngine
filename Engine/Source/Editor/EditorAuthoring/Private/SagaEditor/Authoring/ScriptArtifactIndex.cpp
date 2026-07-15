#include "SagaEditor/Authoring/ScriptArtifactIndex.h"

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>

namespace SagaEditor::Authoring
{
namespace
{

using Json = nlohmann::json;
namespace fs = std::filesystem;

struct KnownArtifact
{
    const char* name;
    const char* fileName;
    bool required;
};

constexpr KnownArtifact kKnownArtifacts[] = {
    {"analysis", "analysis_report.json", true},
    {"projection", "projection_report.json", true},
    {"sourceMap", "source_map.json", true},
    {"runtimeBindings", "runtime_bindings.json", false},
    {"nodeMetadata", "node_metadata.json", false},
    {"patchEvaluation", "patch_evaluation.json", false},
};

[[nodiscard]] int StatusRank(const ScriptArtifactStatus status) noexcept
{
    switch (status)
    {
    case ScriptArtifactStatus::Ready:            return 0;
    case ScriptArtifactStatus::UnknownFreshness: return 1;
    case ScriptArtifactStatus::Stale:            return 2;
    case ScriptArtifactStatus::MissingSource:    return 3;
    case ScriptArtifactStatus::Invalid:          return 4;
    case ScriptArtifactStatus::Missing:          return 5;
    }
    return 5;
}

[[nodiscard]] ScriptArtifactStatus Worse(ScriptArtifactStatus lhs,
                                         ScriptArtifactStatus rhs) noexcept
{
    return StatusRank(lhs) >= StatusRank(rhs) ? lhs : rhs;
}

[[nodiscard]] std::string Sha256Hex(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
    {
        return {};
    }

    using EvpContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
    EvpContext context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1)
    {
        return {};
    }

    std::array<char, 8192> buffer{};
    while (input.good())
    {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0 &&
            EVP_DigestUpdate(
                context.get(),
                buffer.data(),
                static_cast<std::size_t>(count)) != 1)
        {
            return {};
        }
    }
    if (input.bad())
    {
        return {};
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digestLength = 0;
    if (EVP_DigestFinal_ex(context.get(), digest.data(), &digestLength) != 1)
    {
        return {};
    }

    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < digestLength; ++i)
    {
        output << std::setw(2) << static_cast<int>(digest[i]);
    }
    return output.str();
}

[[nodiscard]] Json ReadJsonFile(const fs::path& path,
                                ScriptArtifactEntry& entry)
{
    std::ifstream input(path);
    if (!input.is_open())
    {
        entry.status = ScriptArtifactStatus::Missing;
        entry.diagnostics.push_back(AuthoringDiagnostic{
            "Editor.ScriptArtifact.Missing",
            "script artifact is missing",
            path.string(),
        });
        return Json();
    }

    try
    {
        Json json;
        input >> json;
        if (!json.is_object())
        {
            throw Json::type_error::create(
                302,
                "artifact root is not an object",
                nullptr);
        }
        entry.status = ScriptArtifactStatus::Ready;
        return json;
    }
    catch (const Json::exception& error)
    {
        entry.status = ScriptArtifactStatus::Invalid;
        entry.diagnostics.push_back(AuthoringDiagnostic{
            "Editor.ScriptArtifact.Invalid",
            error.what(),
            path.string(),
        });
        return Json();
    }
}

[[nodiscard]] std::string StringField(const Json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string())
    {
        return {};
    }
    return it->get<std::string>();
}

[[nodiscard]] fs::path ResolveSourcePath(const ProjectReadinessView& project,
                                         const std::string& sourceFile)
{
    fs::path path(sourceFile);
    if (path.is_absolute())
    {
        return path.lexically_normal();
    }
    return (project.projectRoot / path).lexically_normal();
}

[[nodiscard]] ScriptArtifactStatus LoadSourceMapFreshness(
    const ProjectReadinessView& project,
    const Json& json,
    ScriptArtifactEntry& entry)
{
    ScriptArtifactStatus aggregate = ScriptArtifactStatus::Ready;
    if (StringField(json, "sourceHash").empty())
    {
        entry.diagnostics.push_back(AuthoringDiagnostic{
            "Editor.ScriptArtifact.UnknownFreshness",
            "source_map.json does not include an artifact source hash",
            entry.path.string(),
        });
        aggregate = ScriptArtifactStatus::UnknownFreshness;
    }

    const auto filesIt = json.find("files");
    if (filesIt == json.end() || !filesIt->is_array() || filesIt->empty())
    {
        entry.diagnostics.push_back(AuthoringDiagnostic{
            "Editor.ScriptArtifact.UnknownFreshness",
            "source_map.json does not include source file hashes",
            entry.path.string(),
        });
        return ScriptArtifactStatus::UnknownFreshness;
    }

    for (const Json& file : *filesIt)
    {
        if (!file.is_object())
        {
            aggregate = Worse(aggregate, ScriptArtifactStatus::Invalid);
            continue;
        }

        ScriptArtifactSourceStatus source;
        source.sourceFile =
            ResolveSourcePath(project, StringField(file, "sourceFile"));
        source.expectedHash = StringField(file, "sourceHash");

        if (source.expectedHash.empty())
        {
            source.status = ScriptArtifactStatus::UnknownFreshness;
        }
        else if (!fs::exists(source.sourceFile))
        {
            source.status = ScriptArtifactStatus::MissingSource;
        }
        else
        {
            source.actualHash = Sha256Hex(source.sourceFile);
            source.status = source.actualHash == source.expectedHash
                ? ScriptArtifactStatus::Ready
                : ScriptArtifactStatus::Stale;
        }

        aggregate = Worse(aggregate, source.status);
        entry.sources.push_back(source);
    }
    return aggregate;
}

[[nodiscard]] ScriptArtifactStatus LoadGenericFreshness(
    const Json& json,
    const std::string& sourceMapHash)
{
    const std::string artifactHash = StringField(json, "sourceHash");
    if (artifactHash.empty())
    {
        return ScriptArtifactStatus::UnknownFreshness;
    }
    if (sourceMapHash.empty())
    {
        return ScriptArtifactStatus::UnknownFreshness;
    }
    return artifactHash == sourceMapHash
        ? ScriptArtifactStatus::Ready
        : ScriptArtifactStatus::Stale;
}

} // namespace

const char* ToString(const ScriptArtifactStatus status) noexcept
{
    switch (status)
    {
    case ScriptArtifactStatus::Ready:            return "Ready";
    case ScriptArtifactStatus::Missing:          return "Missing";
    case ScriptArtifactStatus::MissingSource:    return "MissingSource";
    case ScriptArtifactStatus::Stale:            return "Stale";
    case ScriptArtifactStatus::Invalid:          return "Invalid";
    case ScriptArtifactStatus::UnknownFreshness: return "UnknownFreshness";
    }
    return "Invalid";
}

ScriptArtifactIndex BuildScriptArtifactIndex(
    const ProjectReadinessView& project)
{
    ScriptArtifactIndex index;
    index.artifactRoot = (project.projectRoot / "Build" / "SagaScript")
        .lexically_normal();

    if (!project.ok)
    {
        index.overallStatus = ScriptArtifactStatus::Invalid;
        index.diagnostics = project.diagnostics;
        return index;
    }

    std::string sourceMapHash;
    fs::path sourceMapPath;
    for (const KnownArtifact& known : kKnownArtifacts)
    {
        if (std::string(known.name) == "sourceMap")
        {
            sourceMapPath = index.artifactRoot / known.fileName;
            ScriptArtifactEntry sourceMap;
            sourceMap.path = sourceMapPath;
            const Json json = ReadJsonFile(sourceMapPath, sourceMap);
            if (sourceMap.status != ScriptArtifactStatus::Missing &&
                sourceMap.status != ScriptArtifactStatus::Invalid)
            {
                sourceMapHash = StringField(json, "sourceHash");
            }
            break;
        }
    }

    index.overallStatus = ScriptArtifactStatus::Ready;
    for (const KnownArtifact& known : kKnownArtifacts)
    {
        ScriptArtifactEntry entry;
        entry.artifactName = known.name;
        entry.path = index.artifactRoot / known.fileName;
        entry.required = known.required;

        const Json json = ReadJsonFile(entry.path, entry);
        if (entry.status != ScriptArtifactStatus::Missing &&
            entry.status != ScriptArtifactStatus::Invalid)
        {
            if (std::string(known.name) == "sourceMap")
            {
                entry.status = LoadSourceMapFreshness(project, json, entry);
            }
            else
            {
                entry.status = LoadGenericFreshness(json, sourceMapHash);
                if (entry.status == ScriptArtifactStatus::UnknownFreshness)
                {
                    entry.diagnostics.push_back(AuthoringDiagnostic{
                        "Editor.ScriptArtifact.UnknownFreshness",
                        "artifact lacks deterministic source freshness evidence",
                        entry.path.string(),
                    });
                }
            }
        }

        if (entry.required)
        {
            index.overallStatus = Worse(index.overallStatus, entry.status);
        }
        index.diagnostics.insert(
            index.diagnostics.end(),
            entry.diagnostics.begin(),
            entry.diagnostics.end());
        index.artifacts.push_back(entry);
    }

    return index;
}

} // namespace SagaEditor::Authoring
