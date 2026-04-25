#pragma once
#include <string>
#include <vector>
#include <cstdint>
namespace SagaEditor {
class IExtensionSerializer {
public:
    virtual ~IExtensionSerializer() = default;
    virtual std::vector<uint8_t> Serialize()                                const = 0;
    virtual bool                 Deserialize(const std::vector<uint8_t>& data)    = 0;
};
} // namespace SagaEditor
