#pragma once
#include <string>
#include <vector>
namespace SagaEditor::Collaboration {
struct MergeConflict { std::string path; std::string local; std::string remote; };
struct MergeResult {
    bool                       success = true;
    std::string                mergedData;
    std::vector<MergeConflict> conflicts;
};
} // namespace SagaEditor::Collaboration
