#pragma once
#include "SagaEditor/Collaboration/Client/PeerInfo.h"
#include <memory>
#include <vector>
namespace SagaEditor::Collaboration {
class PeerRegistry {
public:
    PeerRegistry();
    ~PeerRegistry();
    void AddPeer(const PeerInfo& peer);
    void RemovePeer(uint64_t userId);
    const PeerInfo* FindPeer(uint64_t userId) const noexcept;
    std::vector<PeerInfo> GetOnlinePeers() const;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::Collaboration
