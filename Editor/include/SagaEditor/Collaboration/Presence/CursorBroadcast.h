#pragma once
#include <cstdint>
#include <functional>
namespace SagaEditor::Collaboration {
class CursorBroadcast {
public:
    using BroadcastFn = std::function<void(uint64_t userId, float x, float y)>;
    void SetBroadcastCallback(BroadcastFn fn);
    void Broadcast(uint64_t userId, float x, float y);
private:
    BroadcastFn m_fn;
};
} // namespace SagaEditor::Collaboration
