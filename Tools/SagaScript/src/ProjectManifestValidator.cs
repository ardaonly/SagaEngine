// ProjectManifestValidator.cs
// Resolves the canonical schemaVersion 0 project contract for compilation.

using System.Text.Json;

namespace SagaScript;

internal static class ProjectManifestValidator
{
    public static string ResolveProjectRoot(string input)
    {
        var selected = string.IsNullOrWhiteSpace(input)
            ? Directory.GetCurrentDirectory()
            : Path.GetFullPath(input);

        string manifestPath;
        if (File.Exists(selected))
        {
            if (!string.Equals(Path.GetExtension(selected), ".sagaproj",
                               StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidOperationException(
                    "Explicit project input must be a .sagaproj file.");
            }
            manifestPath = selected;
        }
        else if (Directory.Exists(selected))
        {
            var manifests = Directory.GetFiles(selected, "*.sagaproj",
                                               SearchOption.TopDirectoryOnly);
            Array.Sort(manifests, StringComparer.Ordinal);
            if (manifests.Length != 1)
            {
                throw new InvalidOperationException(manifests.Length == 0
                    ? "Project directory contains no .sagaproj manifest."
                    : "Project directory contains more than one .sagaproj manifest.");
            }
            manifestPath = manifests[0];
        }
        else
        {
            throw new InvalidOperationException(
                $"Project input does not exist: {selected}");
        }

        using var document = JsonDocument.Parse(File.ReadAllText(manifestPath));
        var root = document.RootElement;
        if (root.ValueKind != JsonValueKind.Object ||
            !root.TryGetProperty("schemaVersion", out var schema) ||
            schema.ValueKind != JsonValueKind.Number ||
            !schema.TryGetInt32(out var version) || version != 0)
        {
            throw new InvalidOperationException(
                ".sagaproj requires schemaVersion 0.");
        }
        if (!HasNonEmptyString(root, "projectId") ||
            !HasNonEmptyString(root, "displayName"))
        {
            throw new InvalidOperationException(
                ".sagaproj requires non-empty string projectId and displayName.");
        }

        return Path.GetDirectoryName(Path.GetFullPath(manifestPath))!;
    }

    private static bool HasNonEmptyString(JsonElement root, string property)
    {
        return root.TryGetProperty(property, out var value) &&
               value.ValueKind == JsonValueKind.String &&
               !string.IsNullOrWhiteSpace(value.GetString());
    }
}
