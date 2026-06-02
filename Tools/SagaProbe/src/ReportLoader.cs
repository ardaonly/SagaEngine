using System.Text.Json;
using System.Text.Json.Nodes;

namespace SagaProbe;

internal sealed class ReportLoadException : Exception
{
    public string Code { get; }

    public ReportLoadException(string code, string message) : base(message)
    {
        Code = code;
    }
}

internal static class ReportLoader
{
    public static JsonObject LoadObject(string path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            throw new ReportLoadException(
                "Probe.Input.MissingPath",
                "Report path is required.");
        }

        var fullPath = Path.GetFullPath(path);
        if (!File.Exists(fullPath))
        {
            throw new ReportLoadException(
                "Probe.Input.ReportMissing",
                $"Report does not exist: {fullPath}");
        }

        try
        {
            var node = JsonNode.Parse(File.ReadAllText(fullPath));
            if (node is JsonObject obj)
            {
                return obj;
            }
            throw new ReportLoadException(
                "Probe.Report.RootNotObject",
                "Diagnostics report root must be a JSON object.");
        }
        catch (JsonException e)
        {
            throw new ReportLoadException(
                "Probe.Report.InvalidJson",
                $"Diagnostics report JSON is invalid: {e.Message}");
        }
    }
}
