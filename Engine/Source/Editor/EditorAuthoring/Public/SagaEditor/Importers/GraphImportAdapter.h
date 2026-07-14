#pragma once
#include <string>
namespace SagaEditor {
class GraphImportAdapter {
public:
    bool CanImport(const std::string& path) const noexcept;
    bool Import(const std::string& sourcePath, const std::string& destPath);
};
} // namespace SagaEditor
