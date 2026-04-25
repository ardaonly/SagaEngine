#pragma once
#include <cstdint>
#include <vector>
namespace SagaEditor::Collaboration {
class AuthorityElection {
public:
    uint64_t Elect(const std::vector<uint64_t>& candidates);
};
} // namespace SagaEditor::Collaboration
