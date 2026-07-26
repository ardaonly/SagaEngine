// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <SagaEngine/Render/Backend/IRenderBackend.h>
#include <SagaEngine/UI/UiTypes.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace SagaRuntimeApp
{

struct RuntimeEvidenceCaptureRequest final
{
    std::string name;
    std::uint64_t frameIndex = 0;
    std::string promptBindingLabel;
    std::string promptSemanticLabel;
    bool doorState = false;
    std::size_t uiDrawCallCount = 0;
    std::string priorSemanticCapture;
};

struct RuntimeEvidenceCaptureRecord final
{
    std::string name;
    std::uint64_t frameIndex = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::string promptBindingLabel;
    std::string promptSemanticLabel;
    bool doorState = false;
    std::size_t uiDrawCallCount = 0;
    std::filesystem::path path;
    std::filesystem::path preOverlayPath;
    std::uint64_t preOverlayFrameIndex = 0;
    SagaEngine::UI::UiElementBounds layoutBounds;
    float pixelRatio = 1.0F;
    std::uint32_t hudX = 0;
    std::uint32_t hudY = 0;
    std::uint32_t hudWidth = 0;
    std::uint32_t hudHeight = 0;
    std::uint64_t nonBackgroundPixelCount = 0;
    std::uint64_t baselineDifferencePixelCount = 0;
    std::uint64_t baselineRgbAbsoluteDifference = 0;
    std::uint64_t priorSemanticDifferencePixelCount = 0;
    std::uint64_t priorSemanticRgbAbsoluteDifference = 0;
};

class RuntimeEvidenceSession final
{
public:
    explicit RuntimeEvidenceSession(std::filesystem::path captureDirectory);
    ~RuntimeEvidenceSession();
    [[nodiscard]] bool BeginNamedFrame(
        SagaEngine::Render::Backend::IRenderBackend& backend,
        const RuntimeEvidenceCaptureRequest& request,
        SagaEngine::UI::UiElementBounds layoutBounds,
        float pixelRatio);
    [[nodiscard]] bool EndNamedFrame(
        SagaEngine::Render::Backend::IRenderBackend& backend,
        std::size_t uiDrawCallCount);
    [[nodiscard]] bool HasMeaningfulPixels() const noexcept { return meaningfulPixels_; }
    [[nodiscard]] const std::vector<RuntimeEvidenceCaptureRecord>& Records() const noexcept
    { return records_; }

private:
    struct CapturedPixels;
    struct PendingCapture;
    std::filesystem::path captureDirectory_;
    bool meaningfulPixels_ = false;
    std::vector<RuntimeEvidenceCaptureRecord> records_;
    std::vector<CapturedPixels> pixels_;
    std::unique_ptr<PendingCapture> pending_;
};

} // namespace SagaRuntimeApp
