#pragma once
#include <string>
#include <unordered_map>
namespace SagaEditor {
class LocalizedStringTable {
public:
    void   Load(const std::string& jsonPath);
    std::string Get(const std::string& key) const;
    bool   Has(const std::string& key) const noexcept;
private:
    std::unordered_map<std::string, std::string> m_table;
};
} // namespace SagaEditor
