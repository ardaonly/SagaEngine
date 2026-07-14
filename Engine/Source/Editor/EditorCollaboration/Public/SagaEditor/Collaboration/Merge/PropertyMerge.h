#pragma once
#include "SagaEditor/Collaboration/Merge/IMergeStrategy.h"
namespace SagaEditor::Collaboration {
class PropertyMerge final : public IMergeStrategy {
public:
    MergeResult Merge(const std::string& base,
                       const std::string& local,
                       const std::string& remote) override;
};
} // namespace SagaEditor::Collaboration
