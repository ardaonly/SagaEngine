#pragma once
#include <string>
namespace SagaEditor {
class ProjectManager;
class ProjectStateSerializer {
public:
    static bool Save(const ProjectManager& mgr, const std::string& path);
    static bool Load(ProjectManager& mgr, const std::string& path);
};
} // namespace SagaEditor
