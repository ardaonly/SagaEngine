#pragma once
#include <cstdint>
#include <functional>
#include <string>
namespace SagaEditor {
class InspectorBinder {
public:
    void BindEntity(uint64_t entityId);
    void UnbindAll();
    uint64_t GetBoundEntity() const noexcept;
    void SetOnPropertyChanged(std::function<void(const std::string& path, const std::string& value)> cb);
    void NotifyPropertyChanged(const std::string& path, const std::string& value);
private:
    uint64_t m_entity = 0;
    std::function<void(const std::string&, const std::string&)> m_cb;
};
} // namespace SagaEditor
