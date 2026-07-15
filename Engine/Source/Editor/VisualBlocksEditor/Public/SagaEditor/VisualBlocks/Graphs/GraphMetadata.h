#pragma once
#include <string>
#include <unordered_map>
namespace SagaEditor::VisualBlocks {
struct GraphMetadata {
    std::string name;
    std::string description;
    std::string author;
    std::unordered_map<std::string, std::string> tags;
};
} // namespace SagaEditor::VisualBlocks
