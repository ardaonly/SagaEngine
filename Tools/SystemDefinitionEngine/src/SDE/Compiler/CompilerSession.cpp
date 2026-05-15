/// @file CompilerSession.cpp
/// @brief Project-level compiler facade implementation.

#include "SDE/Compiler/CompilerSession.h"
#include "SDE/Core/StableHash.h"
#include "SDE/IO/JsonModelLoader.h"
#include "SDE/IO/SdeSourceLoader.h"
#include "SDE/Validation/Validator.h"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

namespace SDE
{
namespace
{

std::vector<std::filesystem::path> FindJsonFiles(const std::filesystem::path& dir)
{
    std::vector<std::filesystem::path> paths;
    if (!std::filesystem::exists(dir))
        return paths;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

std::vector<std::filesystem::path> FindSdeFiles(const std::filesystem::path& dir)
{
    std::vector<std::filesystem::path> paths;
    if (!std::filesystem::exists(dir))
        return paths;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".sde")
            paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

bool HasErrors(const std::vector<Diagnostic>& diagnostics)
{
    return std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& d) {
        return d.severity == Severity::Error || d.severity == Severity::Fatal;
    });
}

Diagnostic MakeSessionDiagnostic(Severity severity,
                                 DiagnosticCategory category,
                                 SourceLocation loc,
                                 std::string code,
                                 std::string message)
{
    Diagnostic d = severity == Severity::Fatal
        ? Diagnostic::MakeFatal(std::move(loc), std::move(code), std::move(message))
        : Diagnostic::MakeError(std::move(loc), std::move(code), std::move(message));
    d.category = category;
    return d;
}

DependencyRecord MakeDependencyRecord(const std::filesystem::path& root,
                                      const std::filesystem::path& path,
                                      std::string logicalId)
{
    return {
        std::move(logicalId),
        NormalizePathForArtifactHash(root, path),
        StableFileFingerprint(path),
    };
}

void DetectImportCycles(const std::vector<ModelDefinition>& definitions,
                        std::vector<Diagnostic>& diagnostics)
{
    std::map<std::string, std::vector<std::string>> graph;
    for (const auto& def : definitions)
        graph[def.id] = def.imports;

    std::set<std::string> visiting;
    std::set<std::string> visited;
    std::vector<std::string> stack;

    auto dfs = [&](auto&& self, const std::string& id) -> void {
        if (visited.count(id) != 0)
            return;
        if (visiting.count(id) != 0)
        {
            auto it = std::find(stack.begin(), stack.end(), id);
            std::ostringstream cycle;
            for (; it != stack.end(); ++it)
                cycle << *it << " -> ";
            cycle << id;
            auto d = MakeSessionDiagnostic(Severity::Error, DiagnosticCategory::Import,
                {}, "SDE_IMPORT_CYCLE", "Schema import cycle detected: " + cycle.str() + ".");
            d.metadata["schemaId"] = id;
            diagnostics.push_back(std::move(d));
            return;
        }

        visiting.insert(id);
        stack.push_back(id);
        for (const auto& dep : graph[id])
            self(self, dep);
        stack.pop_back();
        visiting.erase(id);
        visited.insert(id);
    };

    for (const auto& [id, _] : graph)
        dfs(dfs, id);
}

std::string HashRecords(const std::vector<DependencyRecord>& records)
{
    std::ostringstream out;
    for (const auto& record : records)
        out << record.logicalId << ':' << record.normalizedPath << ':' << record.fingerprint << '\n';
    return StableHashBytes(out.str());
}

std::string HashPaths(const std::filesystem::path& root,
                      const std::vector<std::filesystem::path>& paths)
{
    std::ostringstream out;
    for (const auto& path : paths)
    {
        out << NormalizePathForArtifactHash(root, path) << ':'
            << StableFileFingerprint(path) << '\n';
    }
    return StableHashBytes(out.str());
}

std::map<std::string, std::string> HashModels(
    const std::vector<ModelDefinition>& definitions)
{
    std::map<std::string, std::string> hashes;
    for (const auto& def : definitions)
    {
        std::ostringstream out;
        out << def.id << ":v" << def.schemaVersion << '\n';
        for (const auto& field : def.fields)
            out << field.id << ':' << field.type << ':'
                << (field.presence == FieldPresence::Required ? "required" : "optional")
                << '\n';
        hashes[def.id] = StableHashBytes(out.str());
    }
    return hashes;
}

void DetectDuplicates(const std::vector<ModelDefinition>& definitions,
                      const std::vector<ModelInstance>& instances,
                      std::vector<Diagnostic>& diagnostics)
{
    std::set<std::tuple<std::string, int>> models;
    for (const auto& def : definitions)
    {
        const auto key = std::make_tuple(def.id, def.schemaVersion);
        if (!models.insert(key).second)
        {
            auto d = MakeSessionDiagnostic(
                Severity::Error, DiagnosticCategory::Schema, {}, "SDE_DUPLICATE_MODEL",
                "Duplicate model declaration '" + def.id + "' version " +
                std::to_string(def.schemaVersion) + ".");
            d.metadata["modelId"] = def.id;
            d.metadata["modelVersion"] = std::to_string(def.schemaVersion);
            diagnostics.push_back(std::move(d));
        }
    }

    std::set<std::pair<std::string, std::string>> ids;
    for (const auto& instance : instances)
    {
        const auto key = std::make_pair(instance.modelId, instance.instanceId);
        if (!ids.insert(key).second)
        {
            auto d = MakeSessionDiagnostic(
                Severity::Error, DiagnosticCategory::Schema, instance.origin,
                "SDE_DUPLICATE_INSTANCE",
                "Duplicate instance id '" + instance.instanceId +
                "' for model '" + instance.modelId + "'.");
            d.metadata["modelId"] = instance.modelId;
            d.metadata["instanceId"] = instance.instanceId;
            diagnostics.push_back(std::move(d));
        }
    }
}

void SortDiagnostics(std::vector<Diagnostic>& diagnostics)
{
    std::sort(diagnostics.begin(), diagnostics.end(), DiagnosticLess);
}

} // namespace

void SharedRegistrySet::Freeze()
{
    types.Freeze();
    rules.Freeze();
    enums.Freeze();
}

bool SharedRegistrySet::IsFrozen() const noexcept
{
    return types.IsFrozen() && rules.IsFrozen() && enums.IsFrozen();
}

CompilerSession::CompilerSession(ProjectLayout layout)
    : mLayout(std::move(layout))
{
}

ProjectCompilationResult CompilerSession::Validate(CompileContext context)
{
    return Run(false, context);
}

ProjectCompilationResult CompilerSession::Compile(CompileContext context)
{
    return Run(true, context);
}

ProjectCompilationResult CompilerSession::Run(bool buildGraph, CompileContext context)
{
    ProjectCompilationResult result;
    result.versions.compiler = CurrentCompilerVersion();
    result.versions.artifactFormat = CurrentArtifactFormatVersion();
    result.versions.language = CurrentLanguageVersion();
    result.versions.schema = {1, 0, 0};
    result.versions.data = {1, 0, 0};

    if (context.cancellation.IsCancellationRequested())
    {
        result.cancelled = true;
        result.state = CompileState::Aborted;
        result.validation.diagnostics.push_back(MakeSessionDiagnostic(
            Severity::Fatal, DiagnosticCategory::Internal, {}, "SDE_CANCELLED",
            "Compilation was cancelled before project loading."));
        return result;
    }

    const auto root = mLayout.root;
    const auto sourceDir = root / mLayout.sourceDir;
    const auto schemasDir = root / mLayout.schemasDir;
    const auto dataDir = root / mLayout.dataDir;

    SharedRegistrySet registries;
    RuleRegistry::RegisterBuiltIns(registries.rules);

    std::vector<ModelDefinition> definitions;
    std::vector<ModelInstance> instances;

    const auto sourcePaths = FindSdeFiles(sourceDir);
    if (!sourcePaths.empty())
    {
        result.hashes.sourceHash = HashPaths(root, sourcePaths);
        for (const auto& sourcePath : sourcePaths)
        {
            if (context.cancellation.IsCancellationRequested())
            {
                result.cancelled = true;
                result.state = CompileState::Aborted;
                result.validation.diagnostics.push_back(MakeSessionDiagnostic(
                    Severity::Fatal, DiagnosticCategory::Internal, {}, "SDE_CANCELLED",
                    "Compilation was cancelled during .sde source loading."));
                return result;
            }

            auto loaded = Internal::LoadSdeSourceFile(
                sourcePath, registries.types, result.validation.diagnostics);
            for (const auto& file : loaded.files)
            {
                result.versions.language.major =
                    static_cast<uint32_t>(file.languageVersion);
                result.dependencies.directSchemas.push_back(MakeDependencyRecord(
                    root, file.path, file.packageName.empty()
                        ? NormalizePathForArtifactHash(root, file.path)
                        : file.packageName));
            }
            definitions.insert(definitions.end(),
                               std::make_move_iterator(loaded.definitions.begin()),
                               std::make_move_iterator(loaded.definitions.end()));
            instances.insert(instances.end(),
                             std::make_move_iterator(loaded.instances.begin()),
                             std::make_move_iterator(loaded.instances.end()));
        }
    }
    else
    {
        const auto schemaPaths = FindJsonFiles(schemasDir);
        for (const auto& schemaPath : schemaPaths)
        {
            if (context.cancellation.IsCancellationRequested())
            {
                result.cancelled = true;
                result.state = CompileState::Aborted;
                result.validation.diagnostics.push_back(MakeSessionDiagnostic(
                    Severity::Fatal, DiagnosticCategory::Internal, {}, "SDE_CANCELLED",
                    "Compilation was cancelled during schema loading."));
                return result;
            }

            auto definition = JsonModelLoader::LoadDefinition(
                schemaPath.string(), registries.types, result.validation.diagnostics, &registries.enums);
            if (definition)
            {
                if (definition->schemaVersion != 1)
                {
                    result.validation.diagnostics.push_back(MakeSessionDiagnostic(
                        Severity::Error, DiagnosticCategory::Migration, {schemaPath.string(), 0, 0},
                        "SDE_UNSUPPORTED_SCHEMA_VERSION",
                        "Schema '" + definition->id + "' uses unsupported schemaVersion " +
                        std::to_string(definition->schemaVersion) + "."));
                }
                result.dependencies.directSchemas.push_back(MakeDependencyRecord(
                    root, schemaPath, definition->id));
                result.versions.schema.major = static_cast<uint32_t>(definition->schemaVersion);
                definitions.push_back(std::move(*definition));
            }
        }

        const auto dataPaths = FindJsonFiles(dataDir);
        JsonModelLoader loader;
        for (const auto& dataPath : dataPaths)
        {
            if (context.cancellation.IsCancellationRequested())
            {
                result.cancelled = true;
                result.state = CompileState::Aborted;
                result.validation.diagnostics.push_back(MakeSessionDiagnostic(
                    Severity::Fatal, DiagnosticCategory::Internal, {}, "SDE_CANCELLED",
                    "Compilation was cancelled during data loading."));
                return result;
            }

            auto loaded = loader.Load(dataPath.string(), result.validation.diagnostics);
            if (!loaded.empty())
            {
                if (loaded.front().dataVersion != 1)
                {
                    result.validation.diagnostics.push_back(MakeSessionDiagnostic(
                        Severity::Error, DiagnosticCategory::Migration, {dataPath.string(), 0, 0},
                        "SDE_UNSUPPORTED_DATA_VERSION",
                        "Data file for model '" + loaded.front().modelId +
                        "' uses unsupported dataVersion " +
                        std::to_string(loaded.front().dataVersion) + "."));
                }
                result.dependencies.dataFiles.push_back(MakeDependencyRecord(
                    root, dataPath, loaded.front().modelId));
                result.versions.data.major = static_cast<uint32_t>(loaded.front().dataVersion);
            }
            instances.insert(instances.end(),
                             std::make_move_iterator(loaded.begin()),
                             std::make_move_iterator(loaded.end()));
        }
    }

    for (const auto& def : definitions)
    {
        result.versions.schema.major = static_cast<uint32_t>(def.schemaVersion);
        for (const auto& import : def.imports)
            result.dependencies.edges.push_back({def.id, import});
    }
    result.dependencies.transitiveSchemas = result.dependencies.directSchemas;
    DetectImportCycles(definitions, result.validation.diagnostics);
    DetectDuplicates(definitions, instances, result.validation.diagnostics);

    if (HasErrors(result.validation.diagnostics))
    {
        result.state = CompileState::ValidationFailed;
        SortDiagnostics(result.validation.diagnostics);
        return result;
    }

    registries.Freeze();
    result.hashes.modelHashes = HashModels(definitions);
    if (result.hashes.sourceHash.empty())
    {
        result.hashes.sourceHash = StableHashBytes(
            HashRecords(result.dependencies.directSchemas) +
            HashRecords(result.dependencies.dataFiles));
    }
    result.hashes.schemaHash = HashRecords(result.dependencies.directSchemas);
    result.hashes.dataHash = HashRecords(result.dependencies.dataFiles);
    result.hashes.dependencyHash = StableHashDependencyManifest(result.dependencies);

    if (context.cancellation.IsCancellationRequested())
    {
        result.cancelled = true;
        result.state = CompileState::Aborted;
        result.validation.diagnostics.push_back(MakeSessionDiagnostic(
            Severity::Fatal, DiagnosticCategory::Internal, {}, "SDE_CANCELLED",
            "Compilation was cancelled before validation."));
        return result;
    }

    CompileOptions options;
    options.failFast = context.failFast;

    if (buildGraph)
    {
        ModelCompiler compiler(registries.rules, registries.types, &registries.enums, options);
        auto compiled = compiler.Compile(std::move(instances), definitions);
        result.state = compiled.state;
        result.graph = std::move(compiled.graph);
        result.validation.Merge(std::move(compiled.validation));
    }
    else
    {
        Validator validator(registries.rules, registries.types, &registries.enums);
        result.validation.Merge(validator.Validate(instances, definitions));
        result.state = result.validation.state;
    }

    if (result.graph)
        result.hashes.compiledGraphHash = StableHashCompiledGraph(*result.graph);
    result.hashes.artifactHash = StableHashBytes(
        result.hashes.sourceHash +
        result.hashes.schemaHash + result.hashes.dataHash +
        result.hashes.dependencyHash + result.hashes.compiledGraphHash);

    SortDiagnostics(result.validation.diagnostics);
    return result;
}

} // namespace SDE
