#pragma once
#include <string>
namespace SagaEditor::Collaboration {
struct CollaborationToken {
    std::string value;
    uint64_t    expiresAt = 0; ///< Unix ms; 0 = no expiry
    bool        IsValid()  const noexcept;
};
} // namespace SagaEditor::Collaboration
