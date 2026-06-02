namespace SagaLaunchLab;

internal static class LaunchProfileMatrixReporter
{
    private const string ClientPreviewDeferredReason =
        "Client preview is deferred because no bounded ClientHost/runtime preview report seam exists.";

    public static LaunchProfileMatrixReport Build(string projectPath, string binDir)
    {
        var resolver = new LaunchProfileResolver();
        var projectResolution = resolver.ResolveProject(projectPath);
        var diagnostics = projectResolution.Diagnostics.ToList();
        var rows = new List<LaunchProfileMatrixRow>();

        if (projectResolution.Project is not null)
        {
            foreach (var reference in projectResolution.Project.LaunchProfiles.OrderBy(p => p.Id, StringComparer.Ordinal))
            {
                var documentDiagnostics = new List<LaunchDiagnostic>();
                var document = resolver.ReadLaunchProfiles(
                    projectResolution,
                    reference,
                    documentDiagnostics,
                    out _);
                diagnostics.AddRange(documentDiagnostics);

                var profiles = document?.Profiles ?? [];
                var profile = profiles.FirstOrDefault(p => p.Id == reference.Id);
                if (profile is null)
                {
                    rows.Add(new LaunchProfileMatrixRow
                    {
                        ProfileId = reference.Id,
                        Status = "Missing",
                        Reason = "Launch profile id was declared by the project but missing from launch_profiles.json.",
                        Diagnostics =
                        [
                            LaunchProfileResolver.Error(
                                "Launch.Profile.NotFound",
                                "Profile",
                                reference.Id,
                                "Launch profile id was not found in launch_profiles.json."),
                        ],
                    });
                    continue;
                }

                rows.Add(BuildProfileRow(profile, binDir));
            }
        }

        rows.Add(DeferredRow(
            "local-server-plus-one-client",
            "client-preview",
            "one-client-local-preview",
            "Phase 84 one-client launch expectation is deferred without a bounded ClientHost/runtime preview report seam."));
        rows.Add(DeferredRow(
            "local-server-plus-two-clients",
            "client-preview",
            "two-client-local-preview",
            "Phase 84 two-client launch expectation is deferred without a bounded ClientHost/runtime preview report seam."));

        diagnostics.AddRange(rows.SelectMany(row => row.Diagnostics));
        diagnostics = LaunchProfileResolver.Sort(diagnostics);
        return new LaunchProfileMatrixReport
        {
            Status = diagnostics.Any(diagnostic => diagnostic.Severity == "Error")
                ? "Failed"
                : "PassedWithDeferredProfiles",
            Project = new ProjectReportSummary
            {
                ProjectId = projectResolution.Project?.ProjectId ?? "",
                DisplayName = projectResolution.Project?.DisplayName ?? "",
                ProjectRoot = projectResolution.ProjectRoot,
                ManifestPath = projectResolution.ManifestPath,
            },
            Profiles = rows
                .OrderBy(row => row.ProfileId, StringComparer.Ordinal)
                .ToList(),
            Diagnostics = diagnostics,
        };
    }

    private static LaunchProfileMatrixRow BuildProfileRow(LaunchProfile profile, string binDir)
    {
        var diagnostics = new List<LaunchDiagnostic>();
        var executablePath = ResolveExecutable(binDir, profile.Executable);
        var status = "Runnable";
        var reason = "Server/headless launch profile is supported by SagaLaunchLab.";

        if (!string.Equals(profile.Role, "server", StringComparison.Ordinal) ||
            !string.Equals(profile.Mode, "bounded-local-process", StringComparison.Ordinal))
        {
            status = "Deferred";
            reason = "Only server/headless bounded-local-process launch profiles are runnable in this matrix.";
        }
        else if ((Path.IsPathRooted(profile.Executable) || !string.IsNullOrWhiteSpace(binDir)) &&
            !File.Exists(executablePath))
        {
            status = "Blocked";
            reason = "Launch executable does not exist.";
            diagnostics.Add(LaunchProfileResolver.Error(
                "Launch.Process.ExecutableMissing",
                "Process",
                executablePath,
                reason));
        }

        return new LaunchProfileMatrixRow
        {
            ProfileId = profile.Id,
            Role = profile.Role,
            Mode = profile.Mode,
            Status = status,
            ExecutablePath = LaunchProfileResolver.Normalize(executablePath),
            Reason = reason,
            Diagnostics = LaunchProfileResolver.Sort(diagnostics),
        };
    }

    private static LaunchProfileMatrixRow DeferredRow(
        string profileId,
        string role,
        string mode,
        string reason)
    {
        return new LaunchProfileMatrixRow
        {
            ProfileId = profileId,
            Role = role,
            Mode = mode,
            Status = "Deferred",
            Reason = string.IsNullOrWhiteSpace(reason) ? ClientPreviewDeferredReason : reason,
        };
    }

    private static string ResolveExecutable(string binDir, string executable)
    {
        if (Path.IsPathRooted(executable))
        {
            return Path.GetFullPath(executable);
        }
        if (!string.IsNullOrWhiteSpace(binDir))
        {
            return Path.GetFullPath(Path.Combine(binDir, executable));
        }
        return executable;
    }
}
