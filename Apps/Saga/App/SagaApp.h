/// @file SagaApp.h
/// @brief Lifecycle owner for the Saga product orchestration entry point.

#pragma once

#include "App/SagaAppConfig.h"
#include "Processes/SagaProcessLauncher.h"
#include "App/SagaProductHost.h"
#include "Projects/SagaWorkspaceResolver.h"

#include <memory>
#include <ostream>

namespace SagaProduct
{

/// Runs the primary Saga product application.
class SagaApp
{
public:
    SagaApp();
    explicit SagaApp(std::unique_ptr<ISagaProcessLauncher> processLauncher);

    /// Execute the GUI product shell or the explicit prepare-only diagnostic.
    [[nodiscard]] int Run(int argc,
                          char* argv[],
                          const SagaAppConfig& config,
                          std::ostream& out,
                          std::ostream& err);

    /// Execute the explicit prepare-only diagnostic flow.
    [[nodiscard]] int Run(const SagaAppConfig& config,
                          std::ostream& out,
                          std::ostream& err);

private:
    SagaWorkspaceResolver m_workspaceResolver;
    SagaProductHost       m_productHost;
    std::unique_ptr<ISagaProcessLauncher> m_processLauncher;
};

} // namespace SagaProduct
