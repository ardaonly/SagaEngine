#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace SagaEditor {
struct HierarchyNode { uint64_t entityId; std::string name; uint64_t parentId = 0; };
class SceneHierarchyAdapter {
public:
    std::vector<HierarchyNode> GetRootNodes()    const;
    std::vector<HierarchyNode> GetChildren(uint64_t parentId) const;
    void SetParent(uint64_t entityId, uint64_t newParentId);
    void SetOnHierarchyChanged(std::function<void()> cb);
private:
    std::function<void()> m_cb;
};
} // namespace SagaEditor
