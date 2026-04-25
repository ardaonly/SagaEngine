/// @file AudioQueue.h
/// @brief Thread-safe queue that bridges the gameplay thread and the audio thread.
///
/// Gameplay thread pushes AudioEvents after mapping + filtering.
/// Audio thread pops them and dispatches to IAudioBackend (Wwise).
/// Lock-free would be ideal — for now we use a simple mutex queue
/// that is easy to reason about and replace later.

#pragma once

#include "SagaEngine/Audio/AudioEvent.h"

#include <cstdint>
#include <mutex>
#include <vector>

namespace SagaEngine::Audio
{

class AudioQueue
{
public:
    /// Push a single event (gameplay thread).
    void Push(AudioEvent event);

    /// Push a whole batch (gameplay thread).
    void PushBatch(AudioEventBatch& batch);

    /// Drain all pending events into `out` (audio thread).
    /// Returns the number of events drained.
    std::uint32_t Drain(std::vector<AudioEvent>& out);

    /// Number of events waiting (approximate — no lock).
    [[nodiscard]] std::size_t ApproxSize() const noexcept;

    /// Drop everything (e.g. on scene change).
    void Clear();

private:
    mutable std::mutex       m_mutex;
    std::vector<AudioEvent>  m_pending;
};

} // namespace SagaEngine::Audio
