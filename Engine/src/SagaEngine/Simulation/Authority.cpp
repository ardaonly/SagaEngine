#include "SagaEngine/Simulation/Authority.h"
#include "SagaEngine/Core/Log/Log.h"

namespace SagaEngine::Simulation
{

AuthorityManager::AuthorityManager()
{
    LOG_INFO("AuthorityManager", "AuthorityManager initialized");
}

void AuthorityManager::SetServerMode(bool isServer)
{
    m_IsServer = isServer;
    LOG_INFO("AuthorityManager", "Server mode: %s", isServer ? "true" : "false");
}

void AuthorityManager::SetClientId(ClientId id)
{
    m_ClientId = id;
    LOG_DEBUG("AuthorityManager", "Client ID set: %llu", id);
}

AuthorityInfo AuthorityManager::GetAuthority(uint32_t objectId) const
{
    auto it = m_Authorities.find(objectId);
    if (it != m_Authorities.end()) {
        return it->second;
    }
    
    // Default: server has authority
    AuthorityInfo info;
    info.type = m_IsServer ? AuthorityType::Server : AuthorityType::Client;
    return info;
}

void AuthorityManager::SetAuthority(uint32_t objectId, const AuthorityInfo& info)
{
    m_Authorities[objectId] = info;
    
    if (m_OnAuthorityChanged) {
        m_OnAuthorityChanged(objectId, info);
    }
    
    LOG_DEBUG("AuthorityManager", "Authority set for object %u: owner=%llu, type=%d", 
              objectId, info.ownerId, static_cast<int>(info.type));
}

void AuthorityManager::TransferAuthority(uint32_t objectId, ClientId newOwnerId)
{
    auto it = m_Authorities.find(objectId);
    if (it == m_Authorities.end()) {
        LOG_WARN("AuthorityManager", "Cannot transfer authority for unknown object %u", objectId);
        return;
    }
    
    ClientId oldOwner = it->second.ownerId;
    it->second.ownerId = newOwnerId;
    it->second.isMigrating = true;
    
    if (m_OnAuthorityChanged) {
        m_OnAuthorityChanged(objectId, it->second);
    }
    
    LOG_INFO("AuthorityManager", "Authority transferred: object=%u, old=%llu, new=%llu", 
             objectId, oldOwner, newOwnerId);
}

bool AuthorityManager::HasAuthority(uint32_t objectId) const
{
    if (m_IsServer) {
        return true; // Server always has authority
    }
    
    auto it = m_Authorities.find(objectId);
    if (it != m_Authorities.end()) {
        return it->second.ownerId == m_ClientId;
    }
    
    return false;
}

bool AuthorityManager::CanModify(uint32_t objectId) const
{
    auto info = GetAuthority(objectId);
    
    switch (info.type) {
        case AuthorityType::Server:
            return m_IsServer;
        case AuthorityType::Client:
            return info.ownerId == m_ClientId;
        case AuthorityType::Shared:
            return true;
        case AuthorityType::None:
            return false;
    }
    
    return false;
}

void AuthorityManager::SetOnAuthorityChanged(AuthorityChangeCallback cb)
{
    m_OnAuthorityChanged = std::move(cb);
}

} // namespace SagaEngine::Simulation