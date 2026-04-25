#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace SagaEditor {
class PrefabInstanceTracker {
public:
    void TrackInstance(uint64_t entityId, const std::string& prefabPath);
    void UntrackInstance(uint64_t entityId);
    bool IsInstance(uint64_t entityId) const noexcept;
    std::string GetPrefabPath(uint64_t entityId) const;
    std::vector<uint64_t> GetInstances(const std::string& prefabPath) const;
private:
    struct Impl; std::unique_ptr<Impl> m_impl;
};
} // namespace SagaEditor
