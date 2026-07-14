/// @file UiEventQueue.cpp
/// @brief Default backend-neutral UI event queue implementation.

#include "SagaEngine/UI/IUiEventQueue.h"

#include <utility>

namespace SagaEngine::UI
{
namespace
{

class UiEventQueue final : public IUiEventQueue
{
public:
    [[nodiscard]] bool PushEvent(UiEvent event) override
    {
        if (!event.elementId.IsSet())
        {
            diagnostics_.push_back(UiEventDiagnostic{
                UiEventDiagnosticCode::EmptyElementId,
                UiDiagnosticSeverity::Warning,
                "UI event element id is empty.",
                std::move(event),
            });
            return false;
        }

        if (event.sequence == 0)
        {
            event.sequence = nextSequence_++;
        }
        else if (event.sequence >= nextSequence_)
        {
            nextSequence_ = event.sequence + 1;
        }

        events_.push_back(std::move(event));
        return true;
    }

    [[nodiscard]] std::vector<UiEvent> PollEvents() const override
    {
        return events_;
    }

    [[nodiscard]] std::vector<UiEvent> DrainEvents() override
    {
        std::vector<UiEvent> drained;
        drained.swap(events_);
        return drained;
    }

    void Clear() noexcept override
    {
        events_.clear();
        diagnostics_.clear();
    }

    [[nodiscard]] const std::vector<UiEventDiagnostic>& Diagnostics()
        const noexcept override
    {
        return diagnostics_;
    }

    void ClearDiagnostics() noexcept override
    {
        diagnostics_.clear();
    }

private:
    std::vector<UiEvent> events_;
    std::vector<UiEventDiagnostic> diagnostics_;
    UiEventSequence nextSequence_ = 1;
};

} // namespace

std::shared_ptr<IUiEventQueue> CreateUiEventQueue()
{
    return std::make_shared<UiEventQueue>();
}

} // namespace SagaEngine::UI
