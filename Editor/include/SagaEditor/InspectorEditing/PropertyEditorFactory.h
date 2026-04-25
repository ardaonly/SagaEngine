#pragma once
#include <memory>
#include <string>
namespace SagaEditor {
class IPanel;
class PropertyEditorFactory {
public:
    enum class PropertyType { Bool, Int, Float, String, Vec2, Vec3, Vec4, Color, Asset };
    static std::unique_ptr<IPanel> Create(PropertyType type,
                                           const std::string& label,
                                           const std::string& bindingPath);
};
} // namespace SagaEditor
