#include "SagaEngine/ECS/ComponentSerializerRegistry.h"
#include <cstring>
#include <mutex>

namespace SagaEngine::ECS {

ComponentSerializerRegistry& ComponentSerializerRegistry::Instance() {
    static ComponentSerializerRegistry instance;
    return instance;
}

ComponentTypeId ComponentSerializerRegistry::GetComponentTypeId(const char* name) const {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _components.find(name);
    return it != _components.end() ? it->second.id : UINT32_MAX;
}

const char* ComponentSerializerRegistry::GetComponentName(ComponentTypeId id) const {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _idToName.find(id);
    return it != _idToName.end() ? it->second.c_str() : nullptr;
}

const Serializer* ComponentSerializerRegistry::GetSerializer(ComponentTypeId id) const {
    std::lock_guard<std::mutex> lock(_mutex);
    for (const auto& [name, info] : _components) {
        if (info.id == id) {
            return info.serializer.get();
        }
    }
    return nullptr;
}

} // namespace SagaEngine::ECS