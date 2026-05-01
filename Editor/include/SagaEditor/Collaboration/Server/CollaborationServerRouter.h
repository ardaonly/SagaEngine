#pragma once
#include <functional>
#include <memory>
#include <string>
namespace SagaEditor::Collaboration {
class CollaborationServerRouter {
public:
    
    CollaborationServerRouter();
    ~CollaborationServerRouter();
using Handler = std::function<void(uint64_t clientId, const std::string& payload)>;
    void Register(const std::string& messageType, Handler handler);
    void Dispatch(uint64_t clientId, const std::string& messageType, const std::string& payload);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::Collaboration
