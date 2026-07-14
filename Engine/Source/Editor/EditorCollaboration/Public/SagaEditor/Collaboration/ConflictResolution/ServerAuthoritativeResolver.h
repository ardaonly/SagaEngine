#pragma once
#include "SagaEditor/Collaboration/ConflictResolution/IConflictResolver.h"
namespace SagaEditor::Collaboration {
class ServerAuthoritativeResolver final : public IConflictResolver {
public:
    std::string Resolve(const EditConflict& conflict) override;
};
} // namespace SagaEditor::Collaboration
