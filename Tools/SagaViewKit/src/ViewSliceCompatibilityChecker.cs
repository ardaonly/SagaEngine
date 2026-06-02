using System.Text.Json;

namespace SagaViewKit;

internal static class ViewSliceCompatibilityChecker
{
    private static readonly HashSet<string> SupportedViews = new(StringComparer.Ordinal)
    {
        "Simple",
        "Pro",
        "CSharpSource",
        "Diagnostics",
    };

    public static ViewSliceCompatibilityReport Check(string view, string sliceResolutionPath)
    {
        var diagnostics = new List<Diagnostic>();
        if (!SupportedViews.Contains(view))
        {
            diagnostics.Add(Error(
                "View.Slice.UnsupportedView",
                $"Unsupported view for slice compatibility: {view}.",
                view));
        }

        var resources = LoadResources(sliceResolutionPath, diagnostics);
        var hiddenCount = resources.Count(resource => resource.Visibility == "Hidden");
        var restrictedCount = resources.Count(resource =>
            resource.Visibility is "OpaqueRestricted" or "Hidden" ||
            resource.Status is "Restricted" or "Excluded");
        var visibleCount = resources.Count - restrictedCount;

        var blocksSource = view == "CSharpSource" &&
                           resources.Any(resource =>
                               resource.Visibility is "SummaryOnly" or "OpaqueRestricted" or "Hidden");

        if (view == "Simple" && restrictedCount > 0)
        {
            diagnostics.Add(Info(
                "View.Slice.RestrictionsDisclosed",
                "Simple View must disclose hidden or restricted resource counts.",
                sliceResolutionPath));
        }
        if (view == "Pro" && restrictedCount > 0)
        {
            diagnostics.Add(Info(
                "View.Slice.RestrictedMetadataWithheld",
                "Pro View may show diagnostics while keeping restricted entries as placeholders.",
                sliceResolutionPath));
        }
        if (blocksSource)
        {
            diagnostics.Add(Error(
                "View.Slice.CSharpSourceBlocked",
                "CSharpSource View is blocked by slice visibility classification.",
                sliceResolutionPath));
        }
        if (view == "Diagnostics")
        {
            diagnostics.Add(Info(
                "View.Slice.DiagnosticsVisible",
                "Diagnostics View reports slice visibility diagnostics.",
                sliceResolutionPath));
        }

        var failed = diagnostics.Any(d => d.Severity == "Error");
        return new ViewSliceCompatibilityReport
        {
            Status = failed ? "Blocked" : "Passed",
            View = view,
            SliceResolution = Path.GetFullPath(sliceResolutionPath).Replace('\\', '/'),
            Decision = failed ? "Blocked" : "Compatible",
            VisibleCount = visibleCount,
            RestrictedCount = restrictedCount,
            HiddenCount = hiddenCount,
            MutatesSource = false,
            Enforcement = "ReportOnly",
            Diagnostics = diagnostics
                .OrderByDescending(d => d.Severity == "Error")
                .ThenBy(d => d.Code, StringComparer.Ordinal)
                .ThenBy(d => d.Path, StringComparer.Ordinal)
                .ToArray(),
        };
    }

    private static List<SliceResource> LoadResources(
        string sliceResolutionPath,
        List<Diagnostic> diagnostics)
    {
        var resources = new List<SliceResource>();
        try
        {
            using var input = File.OpenRead(sliceResolutionPath);
            using var document = JsonDocument.Parse(input);
            if (!document.RootElement.TryGetProperty("resourceVisibility", out var visibility) ||
                visibility.ValueKind != JsonValueKind.Array)
            {
                diagnostics.Add(Error(
                    "View.Slice.ResourceVisibilityMissing",
                    "Slice resolution report must include resourceVisibility.",
                    sliceResolutionPath));
                return resources;
            }

            foreach (var item in visibility.EnumerateArray())
            {
                resources.Add(new SliceResource(
                    ReadString(item, "status"),
                    ReadString(item, "visibility")));
            }
        }
        catch (JsonException e)
        {
            diagnostics.Add(Error(
                "View.Slice.ParseFailed",
                $"Slice resolution report JSON is invalid: {e.Message}",
                sliceResolutionPath));
        }

        return resources;
    }

    private static string ReadString(JsonElement item, string name)
    {
        return item.TryGetProperty(name, out var value) && value.ValueKind == JsonValueKind.String
            ? value.GetString() ?? ""
            : "";
    }

    private static Diagnostic Error(string code, string message, string path)
    {
        return new Diagnostic
        {
            Severity = "Error",
            Code = code,
            Message = message,
            Path = path,
        };
    }

    private static Diagnostic Info(string code, string message, string path)
    {
        return new Diagnostic
        {
            Severity = "Info",
            Code = code,
            Message = message,
            Path = path,
        };
    }

    private sealed record SliceResource(string Status, string Visibility);
}
