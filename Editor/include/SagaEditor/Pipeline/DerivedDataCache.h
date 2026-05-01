#pragma once
#include <memory>
#include <cstdint>
#include <string>
#include <vector>
namespace SagaEditor {
class DerivedDataCache {
public:
    
    DerivedDataCache();
    ~DerivedDataCache();
bool    Has(const std::string& key)   const noexcept;
    bool    Get(const std::string& key, std::vector<uint8_t>& out) const;
    void    Put(const std::string& key, const std::vector<uint8_t>& data);
    void    Evict(const std::string& key);
    void    Clear();
    size_t  SizeBytes() const noexcept;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
