#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace SagaEditor {
struct PrefabOverride {
    std::string propertyPath;
    std::string overrideValue;
};
class PrefabOverrideResolver {
public:
    std::vector<PrefabOverride> GetOverrides(uint64_t entityId) const;
    void ApplyOverride(uint64_t entityId, const PrefabOverride& ovr);
    void RevertOverride(uint64_t entityId, const std::string& propertyPath);
    void RevertAll(uint64_t entityId);
};
} // namespace SagaEditor
