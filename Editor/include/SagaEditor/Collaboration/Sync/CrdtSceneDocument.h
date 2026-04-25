#pragma once
#include "SagaEditor/Collaboration/Sync/EditOperation.h"
#include <memory>
#include <string>
namespace SagaEditor::Collaboration {
class CrdtSceneDocument {
public:
    CrdtSceneDocument();
    ~CrdtSceneDocument();
    void    Apply(const EditOperation& op);
    std::string Serialize()   const;
    bool    Deserialize(const std::string& data);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor::Collaboration
