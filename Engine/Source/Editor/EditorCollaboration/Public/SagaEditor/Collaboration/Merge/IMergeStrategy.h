#pragma once
#include "SagaEditor/Collaboration/Merge/MergeResult.h"
#include <string>
namespace SagaEditor::Collaboration {
class IMergeStrategy {
public:
    virtual ~IMergeStrategy() = default;
    virtual MergeResult Merge(const std::string& base,
                               const std::string& local,
                               const std::string& remote) = 0;
};
} // namespace SagaEditor::Collaboration
