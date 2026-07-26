// SPDX-License-Identifier: MPL-2.0

#include "RuntimeLaunchSession.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <set>
#include <sstream>

namespace SagaRuntimeApp
{
namespace
{
using Json = nlohmann::json;
namespace Play = SagaRuntime::Play;

std::filesystem::path FindBuildRoot(std::filesystem::path path)
{
    path = path.lexically_normal();
    for (auto cursor = path; !cursor.empty(); cursor = cursor.parent_path())
    {
        if (cursor.filename() == "Build") return cursor;
        if (cursor == cursor.root_path()) break;
    }
    return {};
}

bool Exact(const Json& value, std::initializer_list<std::string_view> fields)
{
    if (!value.is_object()) return false;
    const std::set<std::string_view> allowed(fields);
    for (auto item = value.cbegin(); item != value.cend(); ++item)
        if (!allowed.contains(item.key())) return false;
    return true;
}
}

std::string RuntimeLaunchSession::ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::ostringstream bytes;
    if (input) bytes << input.rdbuf();
    return bytes.str();
}

bool RuntimeLaunchSession::WriteJson(const std::filesystem::path& path, const Json& value)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    const auto temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        const auto bytes = value.dump(2) + "\n";
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!output) return false;
    }
    std::filesystem::rename(temporary, path, error);
    return !error;
}

RuntimeLaunchReadResult RuntimeLaunchSession::ReadEnvelope(const std::filesystem::path& path)
{
    RuntimeLaunchReadResult result;
    Json root;
    try { root = Json::parse(ReadFile(path)); }
    catch (const Json::exception& error)
    {
        result.diagnostics.push_back({"Runtime.Envelope.JsonInvalid", error.what(), path.string()});
        return result;
    }
    if (!Exact(root, {"artifacts", "canonicalManifestHash", "canonicalManifestPath",
                      "environment", "logPath", "playSessionId", "resultPath",
                      "runtimeExecutable", "schemaVersion", "startupReadyPath",
                      "stopPath", "workingDirectory"}) ||
        root.value("schemaVersion", -1) != 1 || !root.contains("artifacts") ||
        !root["artifacts"].is_object() || !root.contains("environment") ||
        !root["environment"].is_object())
    {
        result.diagnostics.push_back({"Runtime.Envelope.SchemaInvalid",
            "Local launch envelope schema version 1 is required.", path.string()});
        return result;
    }
    auto& envelope = result.envelope;
    envelope.schemaVersion = 1;
    envelope.playSessionId = root.value("playSessionId", std::string{});
    envelope.canonicalManifestPath = root.value("canonicalManifestPath", std::string{});
    envelope.canonicalManifestHash = root.value("canonicalManifestHash", std::string{});
    envelope.runtimeExecutable = root.value("runtimeExecutable", std::string{});
    envelope.workingDirectory = root.value("workingDirectory", std::string{});
    envelope.resultPath = root.value("resultPath", std::string{});
    envelope.logPath = root.value("logPath", std::string{});
    envelope.stopPath = root.value("stopPath", std::string{});
    envelope.startupReadyPath = root.value("startupReadyPath", std::string{});
    for (const auto& [id, value] : root["artifacts"].items())
        if (value.is_string() && !id.empty()) envelope.artifacts.emplace(id, value.get<std::string>());
        else result.diagnostics.push_back({"Runtime.Envelope.ArtifactInvalid",
            "Artifact entries require non-empty logical ids and paths.", id});
    for (const auto& [name, value] : root["environment"].items())
        if (value.is_string() && !name.empty()) envelope.environment.emplace(name, value.get<std::string>());
        else result.diagnostics.push_back({"Runtime.Envelope.EnvironmentInvalid",
            "Environment entries require string names and values.", name});
    const auto buildRoot = FindBuildRoot(envelope.workingDirectory);
    auto validation = Play::ValidateLocalEnvelope(envelope,
        {buildRoot, buildRoot.parent_path(), envelope.runtimeExecutable.parent_path()});
    result.diagnostics.insert(result.diagnostics.end(), validation.begin(), validation.end());
    return result;
}

bool RuntimeLaunchSession::VerifyFile(
    const std::map<std::string, std::filesystem::path>& paths,
    const Play::ArtifactIdentity& identity,
    std::vector<Play::RuntimeDiagnostic>& diagnostics)
{
    const auto item = paths.find(identity.logicalId);
    if (item == paths.end() || Play::Sha256Bytes(ReadFile(item->second)) != identity.hash)
    {
        diagnostics.push_back({"Runtime.Launch.ArtifactHashMismatch",
            "Resolved artifact bytes do not match the canonical launch identity.", identity.logicalId});
        return false;
    }
    return true;
}

} // namespace SagaRuntimeApp
