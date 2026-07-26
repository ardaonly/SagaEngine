// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <SagaRuntime/Play/RuntimePlayContracts.hpp>

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace SagaEngine::Render::Backend { class IRenderOverlaySubmission; }

namespace SagaRuntimeApp
{
class RuntimeScriptWorld;
class RuntimeUiSession;

class RuntimeRenderSession final
{
public:
    RuntimeRenderSession(RuntimeScriptWorld& world,
        const std::map<std::string, std::filesystem::path>& artifacts,
        std::vector<SagaRuntime::Play::RuntimeDiagnostic>& diagnostics,
        std::filesystem::path capturePath);
    ~RuntimeRenderSession();

    [[nodiscard]] bool Initialize();
    void Shutdown();
    [[nodiscard]] bool ShouldClose() const noexcept;
    [[nodiscard]] std::vector<std::string> PollLogicalActions(
        const SagaRuntime::Play::RuntimeInputMapArtifact& inputMap, bool injectSmokeKey);
    [[nodiscard]] SagaEngine::Render::Backend::IRenderOverlaySubmission*
        OverlaySubmission() noexcept;
    [[nodiscard]] std::uint32_t Width() const noexcept;
    [[nodiscard]] std::uint32_t Height() const noexcept;
    [[nodiscard]] bool RenderFrame(RuntimeUiSession* ui);
    [[nodiscard]] bool HasMeaningfulPixels() const noexcept;
    [[nodiscard]] std::size_t LastDrawCount() const noexcept;
    [[nodiscard]] const std::filesystem::path& CapturePath() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace SagaRuntimeApp
