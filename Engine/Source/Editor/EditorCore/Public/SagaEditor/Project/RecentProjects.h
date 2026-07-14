#pragma once
#include <string>
#include <vector>
namespace SagaEditor {
class RecentProjects {
public:
    void                      Add(const std::string& path);
    void                      Remove(const std::string& path);
    void                      Clear();
    std::vector<std::string>  GetAll() const;
    void Load(const std::string& configPath);
    void Save(const std::string& configPath) const;
private:
    std::vector<std::string> m_paths;
};
} // namespace SagaEditor
