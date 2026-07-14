#pragma once
#include <string>
namespace SagaEditor {
class BlockGraphImporter {
public:
    bool Import(const std::string& sourcePath, const std::string& destPath);
    bool CanImport(const std::string& path) const noexcept;
};
} // namespace SagaEditor
