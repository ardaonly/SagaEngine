// SourceDiscovery.cs
// Discovers C# source inputs for the standalone SagaScript CLI.

namespace SagaScript;

internal static class SourceDiscovery
{
    public static IReadOnlyList<string> Discover(IReadOnlyList<string> inputs)
    {
        var files = new SortedSet<string>(StringComparer.Ordinal);

        foreach (var input in inputs)
        {
            var fullPath = Path.GetFullPath(input);
            if (File.Exists(fullPath))
            {
                if (Path.GetExtension(fullPath).Equals(".cs", StringComparison.OrdinalIgnoreCase))
                {
                    files.Add(fullPath);
                }

                continue;
            }

            if (Directory.Exists(fullPath))
            {
                foreach (var file in Directory.EnumerateFiles(fullPath, "*.cs", SearchOption.AllDirectories))
                {
                    files.Add(Path.GetFullPath(file));
                }

                continue;
            }

            throw new InvalidOperationException($"Source path does not exist: {input}");
        }

        return files.ToArray();
    }
}
