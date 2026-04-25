#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace SagaEditor {
class IUIMainWindow;
class EditorLayoutSerializer {
public:
    static bool SaveToFile(const IUIMainWindow& win, const std::string& path);
    static bool LoadFromFile(IUIMainWindow& win, const std::string& path);
    static std::vector<uint8_t> Serialize(const IUIMainWindow& win);
    static bool Deserialize(IUIMainWindow& win, const std::vector<uint8_t>& data);
};
} // namespace SagaEditor
