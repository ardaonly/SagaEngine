#pragma once
#include <cstdint>
#include <unordered_map>
namespace SagaEditor::Collaboration {
/// Lamport-style vector clock for per-client sequence tracking.
class OperationVector {
public:
    uint64_t Get(uint64_t userId)      const noexcept;
    void     Increment(uint64_t userId);
    void     Merge(const OperationVector& other);
    bool     HappensBefore(const OperationVector& other) const noexcept;
private:
    std::unordered_map<uint64_t, uint64_t> m_clock;
};
} // namespace SagaEditor::Collaboration
