/// @file SagaLauncherReports.h
/// @brief Read-only known report catalog and distribution status adapter.

#pragma once

#include "Launcher/SagaLauncherModel.h"

#include <filesystem>
#include <optional>

namespace SagaProduct
{

class ISagaReportCatalog
{
public:
    virtual ~ISagaReportCatalog() = default;
    [[nodiscard]] virtual std::vector<SagaLauncherReportReference> Read(
        const std::optional<SagaLauncherProjectSummary>& project) const = 0;
};

class SagaReportCatalog final : public ISagaReportCatalog
{
public:
    explicit SagaReportCatalog(std::filesystem::path launcherReportsRoot,
                               std::filesystem::path evidenceRoot = {});
    [[nodiscard]] std::vector<SagaLauncherReportReference> Read(
        const std::optional<SagaLauncherProjectSummary>& project) const override;

private:
    std::filesystem::path m_launcherReportsRoot;
    std::filesystem::path m_evidenceRoot;
};

class ISagaDistributionStatusReader
{
public:
    virtual ~ISagaDistributionStatusReader() = default;
    [[nodiscard]] virtual SagaLauncherDistributionSummary Read() const = 0;
};

class SagaDistributionStatusReader final : public ISagaDistributionStatusReader
{
public:
    SagaDistributionStatusReader(std::filesystem::path productExecutable,
                                 std::filesystem::path explicitReport = {});
    [[nodiscard]] SagaLauncherDistributionSummary Read() const override;

private:
    std::filesystem::path m_productExecutable;
    std::filesystem::path m_explicitReport;
};

namespace SagaLauncherReportDiagnostics
{
inline constexpr const char* Invalid = "Saga.Launcher.Report.Invalid";
inline constexpr const char* IdentityMismatch = "Saga.Launcher.Distribution.IdentityMismatch";
} // namespace SagaLauncherReportDiagnostics

} // namespace SagaProduct
