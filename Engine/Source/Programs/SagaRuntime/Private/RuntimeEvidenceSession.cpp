// SPDX-License-Identifier: MPL-2.0

#include "RuntimeEvidenceSession.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
#include <optional>

namespace SagaRuntimeApp
{
namespace
{
using Frame = SagaEngine::Render::Backend::RenderFrameCapture;

struct Region final
{
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct Difference final
{
    std::uint64_t changedPixels = 0;
    std::uint64_t rgbAbsolute = 0;
};

bool IsCaptureName(const std::string& value)
{
    return !value.empty() && std::all_of(value.begin(), value.end(),
        [](const unsigned char ch)
        { return std::islower(ch) || std::isdigit(ch) || ch == '-'; });
}

bool IsFrameValid(const Frame& frame)
{
    return frame.width > 0 && frame.height > 0 &&
        frame.rowPitch >= frame.width * 4U &&
        frame.pixels.size() >= static_cast<std::size_t>(frame.rowPitch) * frame.height;
}

std::optional<Region> ToPhysicalRegion(
    const SagaEngine::UI::UiElementBounds& bounds,
    const std::uint32_t width, const std::uint32_t height)
{
    const float right = bounds.x + bounds.width;
    const float bottom = bounds.y + bounds.height;
    if (!std::isfinite(bounds.x) || !std::isfinite(bounds.y) ||
        !std::isfinite(bounds.width) || !std::isfinite(bounds.height) ||
        !std::isfinite(right) || !std::isfinite(bottom) ||
        bounds.width <= 0.0F || bounds.height <= 0.0F ||
        bounds.x < 0.0F || bounds.y < 0.0F ||
        right > static_cast<float>(width) || bottom > static_cast<float>(height))
        return std::nullopt;
    const auto leftPixel = static_cast<std::uint32_t>(std::floor(bounds.x));
    const auto topPixel = static_cast<std::uint32_t>(std::floor(bounds.y));
    const auto rightPixel = std::min(width,
        static_cast<std::uint32_t>(std::ceil(right)));
    const auto bottomPixel = std::min(height,
        static_cast<std::uint32_t>(std::ceil(bottom)));
    if (rightPixel <= leftPixel || bottomPixel <= topPixel) return std::nullopt;
    return Region{leftPixel, topPixel, rightPixel - leftPixel,
                  bottomPixel - topPixel};
}

std::array<std::uint8_t, 3> Rgb(
    const Frame& capture, const std::uint32_t x, const std::uint32_t y)
{
    const auto offset = static_cast<std::size_t>(y) * capture.rowPitch +
        static_cast<std::size_t>(x) * 4U;
    return {capture.pixels[offset], capture.pixels[offset + 1],
            capture.pixels[offset + 2]};
}

std::uint32_t PackedRgb(
    const Frame& capture, const std::uint32_t x, const std::uint32_t y)
{
    const auto value = Rgb(capture, x, y);
    return (static_cast<std::uint32_t>(value[0]) << 16U) |
        (static_cast<std::uint32_t>(value[1]) << 8U) | value[2];
}

std::uint64_t CountNonBackground(const Frame& capture, const Region& region)
{
    std::map<std::uint32_t, std::uint64_t> frequencies;
    for (std::uint32_t y = region.y; y < region.y + region.height; ++y)
        for (std::uint32_t x = region.x; x < region.x + region.width; ++x)
            ++frequencies[PackedRgb(capture, x, y)];
    std::uint64_t background = 0;
    for (const auto& [_, count] : frequencies) background = std::max(background, count);
    return static_cast<std::uint64_t>(region.width) * region.height - background;
}

Difference MeasureDifference(
    const Frame& current, const Frame& reference, const Region& region)
{
    Difference result;
    if (current.width != reference.width || current.height != reference.height)
        return result;
    for (std::uint32_t y = region.y; y < region.y + region.height; ++y)
        for (std::uint32_t x = region.x; x < region.x + region.width; ++x)
        {
            const auto left = Rgb(current, x, y);
            const auto right = Rgb(reference, x, y);
            if (left != right) ++result.changedPixels;
            for (std::size_t channel = 0; channel < left.size(); ++channel)
                result.rgbAbsolute += static_cast<std::uint64_t>(
                    std::abs(static_cast<int>(left[channel]) -
                             static_cast<int>(right[channel])));
        }
    return result;
}

Region UnionRegion(const Region& left, const Region& right)
{
    const auto x = std::min(left.x, right.x);
    const auto y = std::min(left.y, right.y);
    const auto rightEdge = std::max(left.x + left.width, right.x + right.width);
    const auto bottomEdge = std::max(left.y + left.height, right.y + right.height);
    return {x, y, rightEdge - x, bottomEdge - y};
}

bool WritePpm(const std::filesystem::path& path, const Frame& capture)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << "P6\n" << capture.width << " " << capture.height << "\n255\n";
    for (std::uint32_t y = 0; y < capture.height; ++y)
        for (std::uint32_t x = 0; x < capture.width; ++x)
        {
            const auto pixel = static_cast<std::size_t>(y) * capture.rowPitch +
                static_cast<std::size_t>(x) * 4U;
            output.write(reinterpret_cast<const char*>(capture.pixels.data() + pixel), 3);
        }
    return output.good();
}
}

struct RuntimeEvidenceSession::CapturedPixels final
{
    std::string name;
    Frame capture;
    Region region;
};

struct RuntimeEvidenceSession::PendingCapture final
{
    RuntimeEvidenceCaptureRequest request;
    SagaEngine::UI::UiElementBounds bounds;
    float pixelRatio = 1.0F;
    Region region;
    Frame preOverlay;
    std::filesystem::path preOverlayPath;
};

RuntimeEvidenceSession::RuntimeEvidenceSession(std::filesystem::path captureDirectory)
    : captureDirectory_(std::move(captureDirectory)) {}

RuntimeEvidenceSession::~RuntimeEvidenceSession() = default;

bool RuntimeEvidenceSession::BeginNamedFrame(
    SagaEngine::Render::Backend::IRenderBackend& backend,
    const RuntimeEvidenceCaptureRequest& request,
    const SagaEngine::UI::UiElementBounds layoutBounds,
    const float pixelRatio)
{
    if (pending_ || !IsCaptureName(request.name) ||
        !std::isfinite(pixelRatio) || pixelRatio <= 0.0F ||
        std::any_of(records_.begin(), records_.end(),
            [&](const auto& record) { return record.name == request.name; }))
        return false;
    Frame capture;
    if (!backend.CaptureCurrentColorFrame(capture) || !IsFrameValid(capture))
        return false;
    const auto region = ToPhysicalRegion(layoutBounds, capture.width, capture.height);
    if (!region) return false;
    std::error_code error;
    std::filesystem::create_directories(captureDirectory_, error);
    if (error) return false;
    const auto path = captureDirectory_ / (request.name + "-pre-overlay.ppm");
    if (!WritePpm(path, capture)) return false;
    pending_ = std::make_unique<PendingCapture>(PendingCapture{
        request, layoutBounds, pixelRatio, *region, std::move(capture), path});
    return true;
}

bool RuntimeEvidenceSession::EndNamedFrame(
    SagaEngine::Render::Backend::IRenderBackend& backend,
    const std::size_t uiDrawCallCount)
{
    if (!pending_) return false;
    Frame postOverlay;
    if (!backend.CaptureCurrentColorFrame(postOverlay) || !IsFrameValid(postOverlay) ||
        postOverlay.width != pending_->preOverlay.width ||
        postOverlay.height != pending_->preOverlay.height)
    {
        pending_.reset();
        return false;
    }
    const auto path = captureDirectory_ / (pending_->request.name + ".ppm");
    if (!WritePpm(path, postOverlay))
    {
        pending_.reset();
        return false;
    }

    if (records_.empty())
    {
        RuntimeEvidenceCaptureRecord baseline;
        baseline.name = "world-baseline";
        baseline.frameIndex = pending_->request.frameIndex;
        baseline.preOverlayFrameIndex = pending_->request.frameIndex;
        baseline.width = pending_->preOverlay.width;
        baseline.height = pending_->preOverlay.height;
        baseline.path = pending_->preOverlayPath;
        baseline.preOverlayPath = pending_->preOverlayPath;
        baseline.layoutBounds = pending_->bounds;
        baseline.pixelRatio = pending_->pixelRatio;
        baseline.hudX = pending_->region.x;
        baseline.hudY = pending_->region.y;
        baseline.hudWidth = pending_->region.width;
        baseline.hudHeight = pending_->region.height;
        baseline.nonBackgroundPixelCount =
            CountNonBackground(pending_->preOverlay, pending_->region);
        records_.push_back(std::move(baseline));
        pixels_.push_back(
            {"world-baseline", pending_->preOverlay, pending_->region});
    }

    const Difference baselineDifference =
        MeasureDifference(postOverlay, pending_->preOverlay, pending_->region);
    RuntimeEvidenceCaptureRecord record;
    record.name = pending_->request.name;
    record.frameIndex = pending_->request.frameIndex;
    record.preOverlayFrameIndex = pending_->request.frameIndex;
    record.width = postOverlay.width;
    record.height = postOverlay.height;
    record.promptBindingLabel = pending_->request.promptBindingLabel;
    record.promptSemanticLabel = pending_->request.promptSemanticLabel;
    record.doorState = pending_->request.doorState;
    record.uiDrawCallCount = uiDrawCallCount;
    record.path = path;
    record.preOverlayPath = pending_->preOverlayPath;
    record.layoutBounds = pending_->bounds;
    record.pixelRatio = pending_->pixelRatio;
    record.hudX = pending_->region.x;
    record.hudY = pending_->region.y;
    record.hudWidth = pending_->region.width;
    record.hudHeight = pending_->region.height;
    record.nonBackgroundPixelCount = CountNonBackground(postOverlay, pending_->region);
    record.baselineDifferencePixelCount = baselineDifference.changedPixels;
    record.baselineRgbAbsoluteDifference = baselineDifference.rgbAbsolute;

    const auto prior = std::find_if(pixels_.begin(), pixels_.end(),
        [&](const auto& value)
        { return value.name == pending_->request.priorSemanticCapture; });
    if (prior != pixels_.end() && prior->capture.width == postOverlay.width &&
        prior->capture.height == postOverlay.height)
    {
        const auto difference = MeasureDifference(
            postOverlay, prior->capture, UnionRegion(pending_->region, prior->region));
        record.priorSemanticDifferencePixelCount = difference.changedPixels;
        record.priorSemanticRgbAbsoluteDifference = difference.rgbAbsolute;
    }
    meaningfulPixels_ = meaningfulPixels_ ||
        (record.baselineDifferencePixelCount > 0 &&
         record.baselineRgbAbsoluteDifference > 0);
    records_.push_back(std::move(record));
    pixels_.push_back(
        {pending_->request.name, std::move(postOverlay), pending_->region});
    pending_.reset();
    return true;
}

} // namespace SagaRuntimeApp
