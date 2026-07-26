// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <string>
#include <vector>
namespace SagaEditor {
struct ValidationIssue { std::string path; std::string message; bool isError; };
class AssetValidation {
public:
    std::vector<ValidationIssue> Validate(const std::string& assetPath) const;
};
} // namespace SagaEditor
