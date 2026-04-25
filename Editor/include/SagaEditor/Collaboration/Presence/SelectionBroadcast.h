#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace SagaEditor::Collaboration {
class SelectionBroadcast {
public:
    using BroadcastFn = std::function<void(uint64_t userId, const std::vector<std::string>& ids)>;
    void SetBroadcastCallback(BroadcastFn fn);
    void Broadcast(uint64_t userId, const std::vector<std::string>& selectedIds);
private:
    BroadcastFn m_fn;
};
} // namespace SagaEditor::Collaboration
