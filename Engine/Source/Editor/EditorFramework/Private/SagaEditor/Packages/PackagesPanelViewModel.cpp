// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Packages/PackagesPanelViewModel.h"

#include "SagaEditor/Packages/PackageAuthoring.h"

#include <algorithm>
#include <tuple>

namespace SagaEditor
{
void PackagesPanelViewModel::Refresh(const PackageStateCatalog& catalog,
    const ProjectReadinessSession& readiness)
{
    rows_.clear();
    busyOrUnavailable_ = !readiness.PackagePaletteAvailable();
    for (const auto& module : catalog.Modules())
    {
        PackagesPanelRow row;
        row.packageId = module.packageId;
        row.version = module.version;
        row.module = module.module;
        row.state = module.enabled ? "Enabled" : "Available";
        row.runtimeReadiness = module.runtimeReadiness;
        for (std::size_t index = 0; index < module.capabilities.size(); ++index)
        {
            if (index != 0) row.capabilities += ", ";
            row.capabilities += module.capabilities[index];
        }
        row.license = module.license;
        row.trust = module.trust;
        row.canEnable = !busyOrUnavailable_ && !module.enabled;
        row.canDisable = !busyOrUnavailable_ && module.enabled;
        rows_.push_back(std::move(row));
    }
    std::sort(rows_.begin(), rows_.end(),
        [](const auto& left, const auto& right)
        {
            return std::tie(left.packageId, left.module) <
                std::tie(right.packageId, right.module);
        });
}
}
