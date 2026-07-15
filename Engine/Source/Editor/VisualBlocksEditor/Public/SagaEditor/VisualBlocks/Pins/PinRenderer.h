#pragma once
#include "SagaEditor/VisualBlocks/Graphs/GraphDocument.h"
#include <cstdint>
namespace SagaEditor::VisualBlocks {
class PinRenderer {
public:
    void RenderPin(PinId pin, float x, float y, bool connected);
    void RenderLink(float x0,float y0, float x1,float y1, uint32_t color=0xFFFFFFFF);
};
} // namespace SagaEditor::VisualBlocks
