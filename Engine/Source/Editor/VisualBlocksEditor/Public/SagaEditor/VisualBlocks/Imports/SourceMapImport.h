#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
namespace SagaEditor::VisualBlocks {
using SourceMap = std::unordered_map<uint64_t, std::string>; ///< nodeId -> source location
class SourceMapImport {
public:
    bool Load(const std::string& path, SourceMap& out);
};
} // namespace SagaEditor::VisualBlocks
