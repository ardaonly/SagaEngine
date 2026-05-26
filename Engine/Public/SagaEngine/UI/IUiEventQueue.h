/// @file IUiEventQueue.h
/// @brief Backend-neutral UI event queue and sink contract.

#pragma once

#include "SagaEngine/UI/UiTypes.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace SagaEngine::UI
{

/// Stable UI element id exposed to runtime/product code.
struct UiElementId
{
    std::string value;

    [[nodiscard]] static UiElementId FromString(std::string value)
    {
        return UiElementId{std::move(value)};
    }

    [[nodiscard]] bool IsSet() const noexcept
    {
        return !value.empty();
    }
};

/// Monotonic event sequence assigned by the queue.
using UiEventSequence = std::uint64_t;

/// Backend-neutral UI event kind.
enum class UiEventType : std::uint8_t
{
    Click = 0,
    Submit,
    TextChanged,
    FocusGained,
    FocusLost,
};

/// Backend-neutral event emitted by a UI backend.
struct UiEvent
{
    UiEventSequence sequence = 0;
    UiScreenId screenId;
    UiDocumentId documentId;
    UiDocumentHandle document = UiDocumentHandle::kInvalid;
    UiElementId elementId;
    UiEventType type = UiEventType::Click;
    std::string text;
    Input::ModifierFlags modifiers = Input::ModifierFlags::None;
    Input::KeyCode key = Input::KeyCode::Unknown;
    Input::MouseButton mouseButton = Input::MouseButton::Unknown;
    std::int32_t x = 0;
    std::int32_t y = 0;
};

/// Stable event queue diagnostic codes.
enum class UiEventDiagnosticCode : std::uint8_t
{
    None = 0,
    EmptyElementId,
};

/// Structured event bridge diagnostic.
struct UiEventDiagnostic
{
    UiEventDiagnosticCode code = UiEventDiagnosticCode::None;
    UiDiagnosticSeverity severity = UiDiagnosticSeverity::Info;
    std::string message;
    UiEvent event;
};

/// Backend-facing sink used to enqueue UI events without exposing a queue.
class IUiEventSink
{
public:
    virtual ~IUiEventSink() = default;

    /// Push one backend-neutral UI event.
    [[nodiscard]] virtual bool PushEvent(UiEvent event) = 0;
};

/// Runtime-facing deterministic UI event queue.
class IUiEventQueue : public IUiEventSink
{
public:
    ~IUiEventQueue() override = default;

    /// Return queued events without clearing them.
    [[nodiscard]] virtual std::vector<UiEvent> PollEvents() const = 0;

    /// Return queued events in FIFO order and clear the queue.
    [[nodiscard]] virtual std::vector<UiEvent> DrainEvents() = 0;

    /// Clear queued events and diagnostics.
    virtual void Clear() noexcept = 0;

    /// Return event queue diagnostics.
    [[nodiscard]] virtual const std::vector<UiEventDiagnostic>& Diagnostics()
        const noexcept = 0;

    /// Clear event queue diagnostics.
    virtual void ClearDiagnostics() noexcept = 0;
};

/// Create the default backend-neutral UI event queue.
[[nodiscard]] std::shared_ptr<IUiEventQueue> CreateUiEventQueue();

} // namespace SagaEngine::UI
