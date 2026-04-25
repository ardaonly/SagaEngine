#pragma once
#include "SagaEditor/Collaboration/Authority/IAuthorityManager.h"
#include <memory>
namespace SagaEditor::Collaboration {
class AuthorityManager final : public IAuthorityManager {
public:
    AuthorityManager();
    ~AuthorityManager() override;
    bool     IsAuthority()        const noexcept override;
    uint64_t GetAuthorityUserId() const noexcept override;
    void     RequestAuthority()         override;
    void     ReleaseAuthority()         override;
    void     SetOnAuthorityChanged(std::function<void(const AuthorityEvent&)> cb) override;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::Collaboration
