// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Standalone/StandaloneBuildCoordinator.h"
#include "SagaEditor/Performance/BuildProfiles.h"
#include "SagaEditor/Play/PlaySourceSnapshot.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <filesystem>
#include <iostream>
#include <map>

namespace
{
struct Arguments
{
    std::filesystem::path project;
    std::filesystem::path output;
    std::filesystem::path runtime;
    std::string profile;
    std::string runtimeIdentifier;
    std::string configuration = "RelWithDebInfo";
};

bool Parse(int argc, char* argv[], Arguments& result)
{
    std::map<std::string, std::string*> strings = {
        {"--profile", &result.profile}, {"--rid", &result.runtimeIdentifier},
        {"--configuration", &result.configuration}};
    std::map<std::string, std::filesystem::path*> paths = {
        {"--project", &result.project}, {"--output", &result.output},
        {"--runtime", &result.runtime}};
    for (int index = 1; index < argc; ++index)
    {
        const std::string option = argv[index];
        if (option == "--help") return false;
        if (index + 1 >= argc) return false;
        if (const auto found = strings.find(option); found != strings.end())
            *found->second = argv[++index];
        else if (const auto found = paths.find(option); found != paths.end())
            *found->second = argv[++index];
        else return false;
    }
    return !result.project.empty() && !result.output.empty() &&
        !result.runtime.empty() && !result.profile.empty() &&
        !result.runtimeIdentifier.empty();
}
}

int main(int argc, char* argv[])
{
    Arguments arguments;
    if (!Parse(argc, argv, arguments))
    {
        std::cerr << "Usage: SagaStandaloneBuild --project <file> --profile <id> "
                     "--output <directory> --runtime <SagaRuntime> --rid <runtime-id> "
                     "[--configuration <name>]\n";
        return 2;
    }

    SagaEditor::EditorDocumentWorkspace workspace;
    if (!workspace.OpenProject(arguments.project))
    {
        std::cerr << "Standalone.Build.ProjectOpenFailed\n";
        return 3;
    }
    const std::filesystem::path root = workspace.Project()->rootPath;
    const auto profiles = SagaEditor::BuildProfiles::Load(
        root / "BuildProfiles.saga.json", root);
    if (!profiles.succeeded)
    {
        std::cerr << (profiles.diagnostics.empty() ? "BuildProfiles.Invalid" :
            profiles.diagnostics.front().code) << '\n';
        return 4;
    }
    const auto [verified, profileDiagnostics] =
        SagaEditor::BuildProfiles::CreateRunInput(profiles.document, arguments.profile);
    if (!profileDiagnostics.empty())
    {
        std::cerr << profileDiagnostics.front().code << '\n';
        return 5;
    }
    const auto session = "standalone-" + verified.profileHash.substr(
        verified.profileHash.size() > 16 ? verified.profileHash.size() - 16 : 0);
    const auto captured = SagaEditor::CapturePlaySourceSnapshot(workspace, session);
    if (!captured.Succeeded())
    {
        std::cerr << (captured.diagnostics.empty() ? "Build.Snapshot.Failed" :
            captured.diagnostics.front().code) << '\n';
        return 6;
    }

    SagaEditor::StandaloneBuildRequest request;
    request.source = *captured.snapshot;
    request.buildProfilesPath = root / "BuildProfiles.saga.json";
    request.profileId = verified.profileId;
    request.profileHash = verified.profileHash;
    request.runtimeIdentifier = arguments.runtimeIdentifier;
    request.configuration = arguments.configuration;
    request.runtimeExecutable = arguments.runtime;
    request.outputDirectory = arguments.output;
    request.toolchain = SagaEditor::DiscoverScriptAnalysisToolchain();
    request.progress = [](const SagaEditor::StandaloneBuildProgress& progress)
    { std::cerr << progress.message << '\n'; };

    const auto result = SagaEditor::StandaloneBuildCoordinator{}.Build(request);
    std::cout << SagaEditor::StandaloneBuildCoordinator::SerializeReport(result);
    return result.Succeeded() ? 0 : 7;
}
