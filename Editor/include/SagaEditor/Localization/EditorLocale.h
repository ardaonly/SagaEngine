#pragma once
#include <string>
namespace SagaEditor {
class EditorLocale {
public:
    static EditorLocale& Get();
    void SetLanguage(const std::string& languageCode);
    const std::string& GetLanguage() const noexcept;
    std::string Translate(const std::string& key) const;
private:
    std::string m_language = "en";
};
} // namespace SagaEditor
