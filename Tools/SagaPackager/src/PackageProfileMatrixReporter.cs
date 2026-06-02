namespace SagaPackager;

internal static class PackageProfileMatrixReporter
{
    public static PackageProfileMatrixReport Build(
        string projectPath,
        string packageReportPath,
        string diagnosticsSummaryPath)
    {
        var resolver = new ProjectResolver();
        var projectResolution = resolver.ResolveProject(projectPath);
        var diagnostics = projectResolution.Diagnostics.ToList();
        var rows = new List<PackageProfileMatrixRow>();

        if (projectResolution.Project is not null)
        {
            foreach (var reference in projectResolution.Project.PackageProfiles.OrderBy(p => p.Id, StringComparer.Ordinal))
            {
                var documentDiagnostics = new List<PackageDiagnostic>();
                var document = resolver.ReadPackageProfiles(
                    projectResolution,
                    reference,
                    documentDiagnostics,
                    out _);
                diagnostics.AddRange(documentDiagnostics);

                var profileIds = document?.Profiles.Select(profile => profile.Id).ToList() ?? [];
                if (!profileIds.Contains(reference.Id, StringComparer.Ordinal))
                {
                    profileIds.Add(reference.Id);
                }

                foreach (var profileId in profileIds.OrderBy(id => id, StringComparer.Ordinal))
                {
                    var resolved = resolver.Resolve(projectPath, profileId);
                    var rowDiagnostics = resolved.Diagnostics.ToList();
                    var validationStatus = ProjectResolver.HasErrors(rowDiagnostics) ? "Failed" : "Passed";
                    rows.Add(new PackageProfileMatrixRow
                    {
                        ProfileId = profileId,
                        Role = resolved.Profile?.Role ?? "",
                        LaunchProfileId = resolved.Profile?.LaunchProfileId ?? "",
                        ValidationStatus = validationStatus,
                        StageSupport = StageSupport(resolved, validationStatus, rowDiagnostics),
                        PublishCheckEvidenceStatus = PublishCheckEvidenceStatus(packageReportPath, diagnosticsSummaryPath),
                        Diagnostics = ProjectResolver.Sort(rowDiagnostics),
                    });
                }
            }
        }

        if (rows.Count == 0 && !ProjectResolver.HasErrors(diagnostics))
        {
            diagnostics.Add(ProjectResolver.Error(
                "Package.ProfileMatrix.NoProfiles",
                "Profile",
                projectResolution.ManifestPath,
                "Project does not declare package profiles."));
        }

        diagnostics = ProjectResolver.Sort(diagnostics);
        return new PackageProfileMatrixReport
        {
            Status = ProjectResolver.HasErrors(diagnostics) || rows.Any(row => row.ValidationStatus == "Failed")
                ? "Failed"
                : rows.Any(row => row.PublishCheckEvidenceStatus != "EvidenceSupplied")
                    ? "PassedWithMissingPublishEvidence"
                    : "Passed",
            Project = ProjectResolver.ProjectSummary(projectResolution),
            Profiles = rows
                .OrderBy(row => row.ProfileId, StringComparer.Ordinal)
                .ToList(),
            Diagnostics = diagnostics,
        };
    }

    private static string StageSupport(
        ProjectResolution resolved,
        string validationStatus,
        List<PackageDiagnostic> diagnostics)
    {
        if (validationStatus != "Passed" || resolved.Profile is null)
        {
            return "Blocked";
        }
        if (resolved.Profile.Role != "server")
        {
            return "DeferredProfile";
        }

        var outputRoot = Path.GetFullPath(Path.Combine(resolved.ProjectRoot, resolved.Profile.OutputDirectory));
        var allowedRoot = Path.GetFullPath(Path.Combine(resolved.ProjectRoot, "Build", "Packages"));
        if (!IsUnder(outputRoot, allowedRoot))
        {
            diagnostics.Add(ProjectResolver.Error(
                "Package.ProfileMatrix.StageOutputOutsideBuildPackages",
                "Stage",
                outputRoot,
                "Profile outputDirectory is not supported by the bounded stage matrix."));
            return "Blocked";
        }
        return "Supported";
    }

    private static string PublishCheckEvidenceStatus(string packageReportPath, string diagnosticsSummaryPath)
    {
        var hasPackageReport = !string.IsNullOrWhiteSpace(packageReportPath) && File.Exists(packageReportPath);
        var hasDiagnosticsSummary = !string.IsNullOrWhiteSpace(diagnosticsSummaryPath) && File.Exists(diagnosticsSummaryPath);
        if (hasPackageReport && hasDiagnosticsSummary)
        {
            return "EvidenceSupplied";
        }
        return string.IsNullOrWhiteSpace(packageReportPath) && string.IsNullOrWhiteSpace(diagnosticsSummaryPath)
            ? "EvidenceNotSupplied"
            : "EvidenceIncomplete";
    }

    private static bool IsUnder(string path, string root)
    {
        var normalizedPath = Path.GetFullPath(path).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        var normalizedRoot = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        return normalizedPath.StartsWith(normalizedRoot, StringComparison.Ordinal);
    }
}
