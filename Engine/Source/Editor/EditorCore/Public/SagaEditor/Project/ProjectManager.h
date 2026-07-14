#pragma once
#include <functional>
#include <memory>
#include <string>
namespace SagaEditor {
struct ProjectInfo {
    std::string name;
    std::string rootPath;
    std::string engineVersion;
};
class ProjectManager {
public:
    ProjectManager();
    ~ProjectManager();
    bool          OpenProject(const std::string& path);
    void          CloseProject();
    bool          IsProjectOpen() const noexcept;
    const ProjectInfo& GetCurrentProject() const;
    void SetOnProjectChanged(std::function<void()> cb);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
