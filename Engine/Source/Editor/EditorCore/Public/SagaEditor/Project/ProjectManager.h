// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#pragma once
#include "SagaEditor/Project/SagaProjectKitClient.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace SagaEditor {
struct ProjectInfo {
    std::string projectId;
    std::string name;
    std::string rootPath;
    std::string manifestPath;
    std::string engineVersion;
    std::uint32_t schemaVersion = 0;
    std::vector<ValidatedProjectDescriptor::SourceReference> sources;
};
class ProjectManager {
public:
    explicit ProjectManager(std::shared_ptr<IProjectToolClient> client = {});
    ~ProjectManager();
    bool          CreateStarterArena(const ProjectTemplateRequest& request);
    bool          OpenProject(const std::string& path);
    void          CloseProject();
    bool          IsProjectOpen() const noexcept;
    const ProjectInfo& GetCurrentProject() const;
    void SetOnProjectChanged(std::function<void()> cb);
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
