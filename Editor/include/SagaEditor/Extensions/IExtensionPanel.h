#pragma once
#include "SagaEditor/Panels/IPanel.h"
#include "SagaEditor/Extensions/ExtensionPoint.h"
namespace SagaEditor {
class IExtensionPanel : public IPanel {
public:
    virtual ExtensionPoint GetExtensionPoint() const = 0;
};
} // namespace SagaEditor
