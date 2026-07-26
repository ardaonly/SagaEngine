// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <string>
#include <vector>
namespace SagaEditor {
struct ValidationIssue;
class GraphDocument;
class GraphValidation {
public:
    std::vector<ValidationIssue> Validate(const GraphDocument& doc) const;
};
} // namespace SagaEditor
