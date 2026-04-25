#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace SagaEditor {
struct ImportJob {
    std::string sourcePath;
    std::string destPath;
    bool        reimport = false;
};
struct ImportResult {
    bool        success = false;
    std::string assetId;
    std::string errorMessage;
};
class AssetImportManager {
public:
    AssetImportManager();
    ~AssetImportManager();
    ImportResult Import(const ImportJob& job);
    void QueueImport(const ImportJob& job, std::function<void(ImportResult)> cb);
    void ProcessQueue();
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
