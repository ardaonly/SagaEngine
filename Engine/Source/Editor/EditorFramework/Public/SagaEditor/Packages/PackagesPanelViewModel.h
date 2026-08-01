// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <string>
#include <vector>

namespace SagaEditor
{
class PackageStateCatalog;
class ProjectReadinessSession;

struct PackagesPanelRow final
{
    std::string packageId;
    std::string version;
    std::string module;
    std::string state;
    std::string runtimeReadiness;
    std::string capabilities;
    std::string license;
    std::string trust;
    bool canEnable = false;
    bool canDisable = false;
};

class PackagesPanelViewModel final
{
public:
    void Refresh(const PackageStateCatalog& catalog,
        const ProjectReadinessSession& readiness);
    [[nodiscard]] const std::vector<PackagesPanelRow>& Rows() const noexcept
    { return rows_; }
    [[nodiscard]] bool BusyOrUnavailable() const noexcept
    { return busyOrUnavailable_; }

private:
    std::vector<PackagesPanelRow> rows_;
    bool busyOrUnavailable_ = true;
};
}
