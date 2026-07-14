#pragma once
#include <memory>
#include <string>
#include <unordered_map>
namespace SagaEditor {
class LocalizedStringTable;
class TranslationCatalog {
public:
    TranslationCatalog();
    ~TranslationCatalog();
    void   RegisterLanguage(const std::string& code, const std::string& tablePath);
    void   SetActiveLanguage(const std::string& code);
    std::string Translate(const std::string& key) const;
private:
    std::string m_active;
    std::unordered_map<std::string, std::unique_ptr<LocalizedStringTable>> m_tables;
};
} // namespace SagaEditor
