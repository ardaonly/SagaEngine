// SPDX-License-Identifier: MPL-2.0

namespace SagaScript;

internal static class WorkerFileSupport
{
    public static string BuildStagingPath(string outputRoot, string operationId)
    {
        var token = new string(operationId.Where(char.IsLetterOrDigit)
            .Take(24).ToArray());
        if (token.Length == 0) token = "operation";
        var full = Path.GetFullPath(outputRoot);
        var parent = Path.GetDirectoryName(full) ?? Directory.GetCurrentDirectory();
        return Path.Combine(parent, "." + Path.GetFileName(full) +
            ".sagascript-staging-" + token);
    }

    public static void AtomicWrite(string path, byte[] bytes)
    {
        var full = Path.GetFullPath(path);
        Directory.CreateDirectory(Path.GetDirectoryName(full)!);
        var temporary = full + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.WriteAllBytes(temporary, bytes);
            File.Move(temporary, full, overwrite: true);
        }
        finally
        {
            if (File.Exists(temporary)) File.Delete(temporary);
        }
    }
}
