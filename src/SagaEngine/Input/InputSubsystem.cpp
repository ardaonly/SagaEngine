#include "SagaEngine/Input/InputSubsystem.h"
#include "SagaEngine/Input/InputManager.h"
#include "SagaEngine/NativePlatform/PlatformFactory.h"
#include "SagaEngine/NativePlatform/IInputDevice.h"
#include <chrono>
#include <cstring>
#include <atomic>

using namespace Saga::Input;

namespace {

static std::atomic<uint32_t> g_adapterId{1000};

class NativeDeviceAdapter final : public Saga::Input::IInputDevice
{
public:
    NativeDeviceAdapter(std::unique_ptr<::SagaEngine::NativePlatform::IInputDevice> native)
    : m_native(std::move(native))
    , m_id(static_cast<uint32_t>(g_adapterId.fetch_add(1)))
    {
    }

    DeviceId GetId() const override { return m_id; }
    std::string GetName() const override { return m_name.empty() ? "NativeDeviceAdapter" : m_name; }
    void Update() override
    {
        if(!m_native) return;
        m_native->Update();
        while(true) {
            auto ev = m_native->PollEvent();
            if(!ev.has_value()) break;
            Saga::Input::InputEvent e;
            e.device = m_id;
            e.timestamp = static_cast<Saga::Input::Timestamp>(ev->timestamp);
            e.key = static_cast<int>(static_cast<uint32_t>(ev->code));
            e.pressed = ev->pressed;
            e.handled = false;
            if(ev->pressed) e.type = Saga::Input::InputEventType::KeyDown;
            else e.type = Saga::Input::InputEventType::KeyUp;
            if(m_callback) m_callback(e);
        }
    }
    bool IsConnected() const override { return true; }
    void SetEventCallback(EventCallback cb) override { m_callback = std::move(cb); }
    void SetName(const std::string& n) { m_name = n; }

private:
    std::unique_ptr<::SagaEngine::NativePlatform::IInputDevice> m_native;
    DeviceId m_id;
    std::string m_name;
    EventCallback m_callback;
};

} // namespace

InputSubsystem::InputSubsystem(uint64_t clientId)
: m_clientId(clientId)
, m_net(clientId)
, m_subscriberId(0)
{
}

InputSubsystem::~InputSubsystem()
{
    OnShutdown();
}

const char* InputSubsystem::GetName() const
{
    return "InputSubsystem";
}

bool InputSubsystem::OnInit()
{
    auto objs = ::SagaEngine::NativePlatform::CreatePlatformObjects();
    if(objs.window) {
        auto h = objs.window->GetNativeHandle();
        AttachWindow(h);
    }

    if(objs.input) {
        auto adapter = std::make_unique<NativeDeviceAdapter>(std::move(objs.input));
        adapter->SetName("PlatformInput");
        RegisterLocalDevice(std::move(adapter));
    }

    m_subscriberId = Saga::Input::InputManager::Instance().Subscribe([this](const Saga::Input::InputEvent& ev){
        std::lock_guard<std::mutex> lk(m_mutex);
        m_eventCache.push_back(ev);
    });

    m_net.SetSendCallback([this](const Saga::Input::Network::NetworkInputLayer::Packet& p){
        if(m_sendCb) {
            std::vector<uint8_t> copy(p.begin(), p.end());
            m_sendCb(copy);
        }
    });

    return true;
}

void InputSubsystem::OnUpdate(float dt)
{
    std::vector<Saga::Input::InputEvent> recent;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        recent.swap(m_eventCache);
    }
    for(auto &e : recent) TranslateEventToCommand(e);

    auto remote = m_net.ConsumeRemoteCommands();
    for(auto &c : remote) {
        if(m_validator) {
            if(m_validator(c)) {
                if(m_reconciler) m_reconciler(c);
            }
        } else {
            if(m_reconciler) m_reconciler(c);
        }
    }
}

void InputSubsystem::OnShutdown()
{
    if(m_subscriberId) {
        Saga::Input::InputManager::Instance().Unsubscribe(m_subscriberId);
        m_subscriberId = 0;
    }
}

void InputSubsystem::AttachWindow(const ::SagaEngine::NativePlatform::NativeWindowHandle& h)
{
    m_windowHandle = h;
}

void InputSubsystem::RegisterLocalDevice(std::unique_ptr<IInputDevice> d)
{
    if(!d) return;
    Saga::Input::InputManager::Instance().RegisterDevice(std::move(d));
}

void InputSubsystem::SetSendPacketHandler(std::function<void(const std::vector<uint8_t>&)> cb)
{
    m_sendCb = std::move(cb);
}

void InputSubsystem::SetServerValidate(ServerValidate v)
{
    m_validator = std::move(v);
}

void InputSubsystem::SetReconcileCallback(ReconcileCallback r)
{
    m_reconciler = std::move(r);
}

std::vector<Saga::Input::Network::InputCommand> InputSubsystem::SnapshotOutgoingCommands()
{
    return m_net.ConsumeRemoteCommands();
}

std::vector<Saga::Input::Network::InputCommand> InputSubsystem::ConsumeIncomingCommands()
{
    return m_net.ConsumeRemoteCommands();
}

uint64_t InputSubsystem::NowMs() const
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void InputSubsystem::TranslateEventToCommand(const Saga::Input::InputEvent& e)
{
    Saga::Input::Network::InputCommand c{};
    c.seq = m_net.NextSeq();
    c.clientId = m_clientId;
    c.timestamp = NowMs();
    c.type = static_cast<uint16_t>(e.type);
    c.payload = {0,0,0,0};
    if(e.type == Saga::Input::InputEventType::MouseMove) { c.payload[0] = e.mouseX; c.payload[1] = e.mouseY; }
    if(e.type == Saga::Input::InputEventType::Axis) { c.payload[0] = e.axisValue; c.payload[1] = static_cast<float>(e.axisId); }
    if(e.type == Saga::Input::InputEventType::KeyDown || e.type == Saga::Input::InputEventType::KeyUp) { c.payload[0] = static_cast<float>(e.key); c.payload[1] = e.pressed ? 1.0f : 0.0f; }
    m_net.EnqueueLocal(c);
}