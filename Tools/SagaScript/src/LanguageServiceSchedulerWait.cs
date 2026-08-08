// SPDX-License-Identifier: MPL-2.0

namespace SagaScript;

internal static class LanguageServiceSchedulerWait
{
    public static async Task WaitAsync(SemaphoreSlim signal,
        CancellationToken lifetime, TimeSpan delay)
    {
        using var signalCancellation =
            CancellationTokenSource.CreateLinkedTokenSource(lifetime);
        var signalWait = signal.WaitAsync(signalCancellation.Token);
        var delayWait = Task.Delay(delay, lifetime);
        if (await Task.WhenAny(signalWait, delayWait) == signalWait)
        {
            await signalWait;
            return;
        }
        signalCancellation.Cancel();
        try { await signalWait; }
        catch (OperationCanceledException) when (!lifetime.IsCancellationRequested)
        {
        }
    }
}
