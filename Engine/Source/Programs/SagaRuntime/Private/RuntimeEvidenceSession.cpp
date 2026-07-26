// SPDX-License-Identifier: MPL-2.0

#include "RuntimeEvidenceSession.h"

#include <algorithm>
#include <fstream>

namespace SagaRuntimeApp
{

RuntimeEvidenceSession::RuntimeEvidenceSession(std::filesystem::path capturePath)
    : capturePath_(std::move(capturePath)) {}

bool RuntimeEvidenceSession::CaptureMeaningfulFrame(
    SagaEngine::Render::Backend::IRenderBackend& backend)
{
    if (meaningfulPixels_) return true;
    SagaEngine::Render::Backend::RenderFrameCapture capture;
    if (!backend.CaptureCurrentColorFrame(capture) || capture.pixels.size() < 8) return false;
    for (std::size_t offset = 4; offset + 3 < capture.pixels.size(); offset += 4)
    {
        if (std::equal(capture.pixels.begin(), capture.pixels.begin() + 4,
                       capture.pixels.begin() + static_cast<std::ptrdiff_t>(offset))) continue;
        std::ofstream output(capturePath_, std::ios::binary | std::ios::trunc);
        output << "P6\n" << capture.width << " " << capture.height << "\n255\n";
        for (std::uint32_t y = 0; y < capture.height; ++y)
            for (std::uint32_t x = 0; x < capture.width; ++x)
            {
                const auto pixel = static_cast<std::size_t>(y) * capture.rowPitch +
                    static_cast<std::size_t>(x) * 4U;
                output.write(reinterpret_cast<const char*>(capture.pixels.data() + pixel), 3);
            }
        meaningfulPixels_ = output.good();
        return meaningfulPixels_;
    }
    return false;
}

} // namespace SagaRuntimeApp
