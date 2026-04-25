#pragma once
#include <string>
#include <vector>
namespace SagaEditor {
struct BrokenReference { std::string sourcePath; std::string targetId; };
class ReferenceValidator {
public:
    std::vector<BrokenReference> FindBroken(const std::string& projectRoot) const;
    bool IsValid(const std::string& assetId) const noexcept;
};
} // namespace SagaEditor
