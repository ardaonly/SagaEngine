using System.Diagnostics;

namespace SagaLaunchLab;

internal sealed class LaunchProcessRunner
{
    public async Task<LaunchProcessInfo> RunAsync(
        string executable,
        IReadOnlyList<string> arguments,
        string workingDirectory,
        string stdoutPath,
        string stderrPath,
        int timeoutSeconds)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(stdoutPath) ?? ".");
        Directory.CreateDirectory(Path.GetDirectoryName(stderrPath) ?? ".");

        var info = new ProcessStartInfo
        {
            FileName = executable,
            WorkingDirectory = workingDirectory,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
        };
        foreach (var argument in arguments)
        {
            info.ArgumentList.Add(argument);
        }

        using var process = new Process { StartInfo = info };
        var watch = Stopwatch.StartNew();
        process.Start();

        Task<string> stdoutTask = process.StandardOutput.ReadToEndAsync();
        Task<string> stderrTask = process.StandardError.ReadToEndAsync();
        Task waitTask = process.WaitForExitAsync();
        Task timeoutTask = Task.Delay(TimeSpan.FromSeconds(timeoutSeconds));

        var timedOut = await Task.WhenAny(waitTask, timeoutTask) == timeoutTask;
        if (timedOut)
        {
            try
            {
                process.Kill(entireProcessTree: true);
            }
            catch (InvalidOperationException)
            {
            }
            await process.WaitForExitAsync();
        }

        watch.Stop();
        await File.WriteAllTextAsync(stdoutPath, await stdoutTask);
        await File.WriteAllTextAsync(stderrPath, await stderrTask);

        return new LaunchProcessInfo
        {
            Executable = LaunchProfileResolver.Normalize(executable),
            Arguments = arguments.ToList(),
            WorkingDirectory = LaunchProfileResolver.Normalize(workingDirectory),
            ExitCode = timedOut ? null : process.ExitCode,
            TimedOut = timedOut,
            DurationMs = watch.ElapsedMilliseconds,
        };
    }
}
