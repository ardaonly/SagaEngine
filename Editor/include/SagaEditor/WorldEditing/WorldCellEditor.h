#pragma once
#include <cstdint>
#include <string>
namespace SagaEditor {
class WorldCellEditor {
public:
    void SelectCell(int32_t x, int32_t y);
    void DeselectCell();
    bool IsCellSelected() const noexcept;
    void SetCellProperty(const std::string& key, const std::string& value);
    std::string GetCellProperty(const std::string& key) const;
private:
    bool m_selected = false;
    int32_t m_x = 0, m_y = 0;
};
} // namespace SagaEditor
