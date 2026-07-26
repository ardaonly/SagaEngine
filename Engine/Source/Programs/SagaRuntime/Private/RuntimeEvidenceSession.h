// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <SagaEngine/Render/Backend/IRenderBackend.h>

#include <filesystem>

namespace SagaRuntimeApp
{

class RuntimeEvidenceSession final
{
public:
    explicit RuntimeEvidenceSession(std::filesystem::path capturePath);
    [[nodiscard]] bool CaptureMeaningfulFrame(
        SagaEngine::Render::Backend::IRenderBackend& backend);
    [[nodiscard]] bool HasMeaningfulPixels() const noexcept { return meaningfulPixels_; }
    [[nodiscard]] const std::filesystem::path& CapturePath() const noexcept { return capturePath_; }

private:
    std::filesystem::path capturePath_;
    bool meaningfulPixels_ = false;
};

} // namespace SagaRuntimeApp
