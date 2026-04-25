#pragma once
#include "SagaEditor/Extensions/ExtensionManifest.h"
#include <memory>
#include <string>
namespace SagaEditor {
class IEditorExtension;
class ExtensionLoader {
public:
    ExtensionLoader();
    ~ExtensionLoader();
    std::unique_ptr<IEditorExtension> Load(const ExtensionManifest& manifest);
    void Unload(const std::string& extensionId);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
