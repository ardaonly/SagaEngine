#pragma once
#include <cstdint>
namespace SagaEditor::Collaboration {
class IAuthorityManager;
class AuthorityHandoff {
public:
    explicit AuthorityHandoff(IAuthorityManager& mgr);
    bool Initiate(uint64_t targetUserId);
private:
    IAuthorityManager& m_mgr;
};
} // namespace SagaEditor::Collaboration
