#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace SagaEditor {
struct ValidationIssue;
class PrefabValidation {
public:
    std::vector<ValidationIssue> Validate(uint64_t entityId)       const;
    std::vector<ValidationIssue> ValidatePrefab(const std::string& path) const;
};
} // namespace SagaEditor
