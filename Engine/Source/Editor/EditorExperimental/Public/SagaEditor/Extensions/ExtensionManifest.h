#pragma once
#include <string>
#include <vector>
namespace SagaEditor {
struct ExtensionManifest {
    std::string              id;
    std::string              displayName;
    std::string              version;
    std::string              author;
    std::string              entryLibrary;   ///< Shared library path (relative)
    std::vector<std::string> dependencies;
};
} // namespace SagaEditor
