#pragma once
#include <string>
namespace SagaEditor {
class IExtensionCommand {
public:
    virtual ~IExtensionCommand() = default;
    virtual std::string GetCommandId()   const = 0;
    virtual std::string GetLabel()       const = 0;
    virtual void        Execute()              = 0;
    virtual bool        IsEnabled()      const noexcept { return true; }
};
} // namespace SagaEditor
