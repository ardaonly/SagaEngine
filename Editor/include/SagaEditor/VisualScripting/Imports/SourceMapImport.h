#pragma once
#include <string>
#include <unordered_map>
namespace SagaEditor::VisualScripting {
using SourceMap = std::unordered_map<uint64_t, std::string>; ///< nodeId -> source location
class SourceMapImport {
public:
    bool Load(const std::string& path, SourceMap& out);
};
} // namespace SagaEditor::VisualScripting
