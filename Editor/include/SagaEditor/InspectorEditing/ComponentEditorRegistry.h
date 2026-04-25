#pragma once
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
namespace SagaEditor {
class IPanel;
class ComponentEditorRegistry {
public:
    using FactoryFn = std::function<std::unique_ptr<IPanel>()>;
    void    Register(std::type_index compType, FactoryFn factory);
    IPanel* GetEditor(std::type_index compType) const;
private:
    std::unordered_map<std::type_index, FactoryFn> m_factories;
};
} // namespace SagaEditor
