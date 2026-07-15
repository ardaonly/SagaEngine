#pragma once
#include <string>
namespace SagaEditor::VisualBlocks {
class GraphDocument;
class CompileEvaluationAdapter {
public:
    std::string GenerateEvaluation(const GraphDocument& doc) const;
};
} // namespace SagaEditor::VisualBlocks
