#pragma once
#include <memory>
#include <string>
#include <vector>
namespace SagaEditor {
class ContentPipeline {
public:
    ContentPipeline();
    ~ContentPipeline();
    void AddSourceDirectory(const std::string& path);
    void Build();
    void Rebuild();
    bool IsBuilding() const noexcept;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
