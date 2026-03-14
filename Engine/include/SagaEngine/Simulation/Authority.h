#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace SagaEngine::Simulation
{

using ClientId = uint64_t;

enum class AuthorityType : uint8_t
{
    Server,      // Server has full authority
    Client,      // Client has local authority
    Shared,      // Both can modify (with conflict resolution)
    None         // Read-only
};

struct AuthorityInfo
{
    ClientId ownerId{0};
    AuthorityType type{AuthorityType::None};
    bool isMigrating{false};
};

class AuthorityManager
{
public:
    AuthorityManager();
    
    void SetServerMode(bool isServer);
    bool IsServer() const { return m_IsServer; }
    
    void SetClientId(ClientId id);
    ClientId GetClientId() const { return m_ClientId; }
    
    AuthorityInfo GetAuthority(uint32_t objectId) const;
    void SetAuthority(uint32_t objectId, const AuthorityInfo& info);
    void TransferAuthority(uint32_t objectId, ClientId newOwnerId);
    
    bool HasAuthority(uint32_t objectId) const;
    bool CanModify(uint32_t objectId) const;
    
    using AuthorityChangeCallback = std::function<void(uint32_t, const AuthorityInfo&)>;
    void SetOnAuthorityChanged(AuthorityChangeCallback cb);
    
private:
    bool m_IsServer{false};
    ClientId m_ClientId{0};
    std::unordered_map<uint32_t, AuthorityInfo> m_Authorities;
    AuthorityChangeCallback m_OnAuthorityChanged;
};

} // namespace SagaEngine::Simulation