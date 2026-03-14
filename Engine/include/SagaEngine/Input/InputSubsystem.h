#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <cstdint>
#include <optional>
#include <mutex>
#include "SagaEngine/Core/Subsystem/ISubsystem.h"
#include "SagaEngine/Input/Events/InputEvent.h"
#include "SagaEngine/Input/Devices/IInputDevice.h"
#include "SagaEngine/Input/InputManager.h"
#include "SagaEngine/Platform/IWindow.h"
#include "SagaEngine/Input/Network/InputCommand.h"
#include "SagaEngine/Input/Network/NetworkInputLayer.h"

namespace Saga::Input
{

class InputSubsystem final : public ::SagaEngine::Core::ISubsystem
{
public:
    using ServerValidate = std::function<bool(const Saga::Input::Network::InputCommand&)>;
    using ReconcileCallback = std::function<void(const Saga::Input::Network::InputCommand&)>;

    InputSubsystem(uint64_t clientId = 0);
    ~InputSubsystem() override;

    const char* GetName() const override;
    bool OnInit() override;
    void OnUpdate(float dt) override;
    void OnShutdown() override;

    void AttachWindow(const ::SagaEngine::Platform::NativeWindowHandle& h);
    void RegisterLocalDevice(std::unique_ptr<IInputDevice> d);
    void SetSendPacketHandler(std::function<void(const std::vector<uint8_t>&)> cb);
    void SetServerValidate(ServerValidate v);
    void SetReconcileCallback(ReconcileCallback r);

    std::vector<Saga::Input::Network::InputCommand> SnapshotOutgoingCommands();
    std::vector<Saga::Input::Network::InputCommand> ConsumeIncomingCommands();

private:
    uint64_t m_clientId;
    Saga::Input::Network::NetworkInputLayer m_net;
    std::vector<Saga::Input::InputEvent> m_eventCache;
    std::mutex m_mutex;
    SubscriberId m_subscriberId{0};
    std::function<void(const std::vector<uint8_t>&)> m_sendCb;
    ServerValidate m_validator;
    ReconcileCallback m_reconciler;
    std::optional<::SagaEngine::Platform::NativeWindowHandle> m_windowHandle;

    uint64_t NowMs() const;
    void TranslateEventToCommand(const Saga::Input::InputEvent& e);
};

} // namespace Saga::Input