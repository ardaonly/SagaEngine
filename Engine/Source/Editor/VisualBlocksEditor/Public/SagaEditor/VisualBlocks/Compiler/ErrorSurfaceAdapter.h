#pragma once
#include "SagaEditor/VisualBlocks/Graphs/GraphDocument.h"
#include <string>
#include <vector>
namespace SagaEditor::VisualBlocks {
struct CompileError { NodeId nodeId; std::string message; };
class ErrorSurfaceAdapter {
public:
    void SetErrors(const std::vector<CompileError>& errors);
    void ClearErrors();
    const std::vector<CompileError>& GetErrors() const noexcept;
private:
    std::vector<CompileError> m_errors;
};
} // namespace SagaEditor::VisualBlocks
