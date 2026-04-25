#pragma once
#include "SagaEditor/VisualScripting/Graphs/GraphDocument.h"
namespace SagaEditor::VisualScripting {
class PinRenderer {
public:
    void RenderPin(PinId pin, float x, float y, bool connected);
    void RenderLink(float x0,float y0, float x1,float y1, uint32_t color=0xFFFFFFFF);
};
} // namespace SagaEditor::VisualScripting
