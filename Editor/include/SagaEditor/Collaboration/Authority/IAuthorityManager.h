#pragma once
#include <cstdint>
#include <functional>
namespace SagaEditor::Collaboration {
struct AuthorityEvent;
class IAuthorityManager {
public:
    virtual ~IAuthorityManager() = default;
    virtual bool     IsAuthority()          const noexcept = 0;
    virtual uint64_t GetAuthorityUserId()   const noexcept = 0;
    virtual void     RequestAuthority()                    = 0;
    virtual void     ReleaseAuthority()                    = 0;
    virtual void     SetOnAuthorityChanged(std::function<void(const AuthorityEvent&)> cb) = 0;
};
} // namespace SagaEditor::Collaboration
