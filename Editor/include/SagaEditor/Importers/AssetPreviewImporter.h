#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace SagaEditor {
struct AssetPreview {
    uint32_t             width  = 0;
    uint32_t             height = 0;
    std::vector<uint8_t> pixelsRGBA;
};
class AssetPreviewImporter {
public:
    AssetPreview GeneratePreview(const std::string& assetPath, uint32_t maxSize = 128);
};
} // namespace SagaEditor
