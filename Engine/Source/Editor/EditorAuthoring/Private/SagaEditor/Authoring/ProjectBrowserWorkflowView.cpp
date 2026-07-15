#include "SagaEditor/Authoring/ProjectBrowserWorkflowView.h"

#include <filesystem>
#include <utility>

namespace SagaEditor::Authoring
{
namespace
{

namespace fs = std::filesystem;

[[nodiscard]] bool Exists(const fs::path& path)
{
    return !path.empty() && fs::exists(path);
}

[[nodiscard]] ProjectBrowserArtifactLink Link(
    const std::string& id,
    const std::string& kind,
    const fs::path& path)
{
    const bool exists = Exists(path);
    return ProjectBrowserArtifactLink{id, kind, path, exists, exists};
}

void AddDiagnostic(ProjectBrowserWorkflowView& view,
                   ProjectBrowserSectionView& section,
                   const std::string& code,
                   const std::string& message,
                   const fs::path& path)
{
    AuthoringDiagnostic diagnostic{code, message, path.string()};
    section.diagnostics.push_back(diagnostic);
    view.diagnostics.push_back(std::move(diagnostic));
}

ProjectBrowserSectionView MakeSection(const std::string& name,
                                      const fs::path& path)
{
    ProjectBrowserSectionView section;
    section.name = name;
    section.path = path.lexically_normal();
    section.exists = Exists(section.path);
    section.openableArtifactPath = section.path;
    section.status = section.exists ? "Ready" : "Missing";
    return section;
}

void AddReferenceLinks(ProjectBrowserSectionView& section,
                       const std::string& kind,
                       const std::vector<ProjectReadinessPathReference>& refs)
{
    for (const ProjectReadinessPathReference& ref : refs)
    {
        section.artifactLinks.push_back(Link(ref.id, kind, ref.path));
    }
}

} // namespace

ProjectBrowserWorkflowView LoadProjectBrowserWorkflowView(
    const std::filesystem::path& manifestPath)
{
    const ProjectReadinessView project =
        LoadProjectReadinessView(manifestPath);

    ProjectBrowserWorkflowView view;
    view.ok = project.ok;
    view.projectId = project.projectId;
    view.displayName = project.displayName;
    view.projectRoot = project.projectRoot;
    view.manifestPath = project.manifestPath;
    view.diagnosticsPath = project.diagnosticsPath;
    view.generatedReportsPath = project.generatedReportsPath;
    view.scriptFolders = project.scriptFolders;
    view.launchProfiles = project.launchProfiles;
    view.packageProfiles = project.packageProfiles;
    view.diagnostics = project.diagnostics;

    if (!project.ok)
    {
        return view;
    }

    ProjectBrowserSectionView scenes =
        MakeSection("Scenes", project.projectRoot / "Scenes");
    if (!scenes.exists)
    {
        AddDiagnostic(
            view,
            scenes,
            "Editor.ProjectBrowser.SceneRootMissing",
            "scene root is missing; no scene source-of-truth was discovered",
            scenes.path);
    }

    ProjectBrowserSectionView scripts =
        MakeSection("Scripts",
                    project.scriptFolders.empty()
                        ? project.projectRoot / "Scripts"
                        : project.scriptFolders.front().path);
    AddReferenceLinks(scripts, "scriptFolder", project.scriptFolders);
    if (project.scriptFolders.empty())
    {
        AddDiagnostic(
            view,
            scripts,
            "Editor.ProjectBrowser.ScriptFoldersMissing",
            "project manifest does not list script folders",
            project.manifestPath);
    }
    for (const ProjectReadinessPathReference& folder : project.scriptFolders)
    {
        if (!Exists(folder.path))
        {
            AddDiagnostic(
                view,
                scripts,
                "Editor.ProjectBrowser.ScriptFolderMissing",
                "script folder listed by the project manifest is missing",
                folder.path);
            scripts.status = "Missing";
            scripts.exists = false;
        }
    }

    ProjectBrowserSectionView assets =
        MakeSection("Assets", project.projectRoot / "Assets");
    if (!assets.exists)
    {
        AddDiagnostic(
            view,
            assets,
            "Editor.ProjectBrowser.AssetRootMissing",
            "asset root is missing",
            assets.path);
    }

    ProjectBrowserSectionView packages =
        MakeSection("Packages",
                    project.packageProfiles.empty()
                        ? project.projectRoot / "package_profiles.json"
                        : project.packageProfiles.front().path);
    AddReferenceLinks(packages, "packageProfile", project.packageProfiles);
    if (project.packageProfiles.empty())
    {
        AddDiagnostic(
            view,
            packages,
            "Editor.ProjectBrowser.PackageProfilesMissing",
            "project manifest does not list package profiles",
            project.manifestPath);
    }
    for (const ProjectReadinessPathReference& profile : project.packageProfiles)
    {
        if (!Exists(profile.path))
        {
            AddDiagnostic(
                view,
                packages,
                "Editor.ProjectBrowser.PackageProfileMissing",
                "package profile listed by the project manifest is missing",
                profile.path);
            packages.status = "Missing";
            packages.exists = false;
        }
    }

    ProjectBrowserSectionView reports =
        MakeSection("Reports",
                    project.generatedReportsPath.empty()
                        ? project.projectRoot / "Build" / "Reports"
                        : project.generatedReportsPath);
    reports.artifactLinks.push_back(
        Link("diagnostics", "diagnosticsRoot", project.diagnosticsPath));
    reports.artifactLinks.push_back(
        Link("generatedReports", "reportsRoot", reports.path));
    AddReferenceLinks(reports, "launchProfile", project.launchProfiles);
    for (const ProjectReadinessPathReference& profile : project.launchProfiles)
    {
        if (!Exists(profile.path))
        {
            AddDiagnostic(
                view,
                reports,
                "Editor.ProjectBrowser.LaunchProfileMissing",
                "launch profile listed by the project manifest is missing",
                profile.path);
        }
    }
    if (!reports.exists)
    {
        AddDiagnostic(
            view,
            reports,
            "Editor.ProjectBrowser.ReportsRootMissing",
            "generated reports root is missing",
            reports.path);
    }

    view.sections = {scenes, scripts, assets, packages, reports};
    view.ok = view.diagnostics.empty();
    return view;
}

} // namespace SagaEditor::Authoring
