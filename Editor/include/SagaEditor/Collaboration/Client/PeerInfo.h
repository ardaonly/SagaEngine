#pragma once
#include <cstdint>
#include <string>
namespace SagaEditor::Collaboration {
struct PeerInfo {
    uint64_t    userId   = 0;
    std::string username;
    std::string avatarUrl;
    bool        isOnline = false;
};
} // namespace SagaEditor::Collaboration
