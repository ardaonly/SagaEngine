#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace SagaEditor {
struct AssetEntry { std::string assetId; std::string path; std::string type; };
class ProjectIndexer {
public:
    ProjectIndexer();
    ~ProjectIndexer();
    void Index(const std::string& rootPath);
    void Reindex();
    std::vector<AssetEntry> FindByType(const std::string& type) const;
    std::vector<AssetEntry> Search(const std::string& query) const;
    void SetOnIndexed(std::function<void()> cb);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
