#pragma once
#include <cstdint>
#include <string>
namespace SagaEditor::Collaboration {
struct LockToken {
    uint64_t    lockId   = 0;
    uint64_t    userId   = 0;
    std::string objectId;
    uint64_t    expiresAt = 0; ///< Unix ms; 0 = no expiry
};
} // namespace SagaEditor::Collaboration
