// SPDX-License-Identifier: MPL-2.0
// ProjectManifestValidator.cs
// Resolves projects through the SagaProjectKit-owned shared contract.

using SagaProjectKit.Core;

namespace SagaScript;

internal static class ProjectManifestValidator
{
    public static string ResolveProjectRoot(string input)
    {
        var selected = string.IsNullOrWhiteSpace(input) ? Directory.GetCurrentDirectory() : input;
        var load = new ProjectLoader().Load(selected, checkReferences: false);
        var blocking = load.Diagnostics
            .Where(item => item.Code != "Project.Manifest.MissingField")
            .ToArray();
        // Legacy loose-source compilation historically accepted the minimal
        // schema-0 identity document. Project-driven commands use the full
        // shared validation result and do not take this compatibility path.
        if (load.Manifest is null || blocking.Length != 0)
        {
            var message = string.Join("; ", blocking.Select(item => $"{item.Code}: {item.Message}"));
            throw new InvalidOperationException(message.Length == 0 ? "Project manifest is invalid." : message);
        }
        return load.ProjectRoot;
    }
}
