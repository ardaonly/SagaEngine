/*

==  NOTE: This file (main.cpp) is currently used as a *temporary* debug area.
==  Purpose:
==    - Quick local testing of platform, input and early RHI bits.
==    - Short-lived experiments and manual runtime checks.

*/
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <queue>
#include <optional>
#include <memory>
#include <random>
#include "SagaEngine/Input/InputManager.h"
#include "SagaEngine/Input/Events/InputEvent.h"
#include "SagaEngine/Input/Devices/IInputDevice.h"
#include "SagaEngine/Input/Network/NetworkInputLayer.h"
#include "SagaEngine/Input/Network/InputCommand.h"

using namespace Saga::Input;
using namespace Saga::Input::Network;

class SimulatedDevice : public IInputDevice
{
public:
    SimulatedDevice(DeviceId id, const std::string& name)
    : m_id(id)
    , m_name(name)
    , m_connected(true)
    , m_start(std::chrono::steady_clock::now())
    , m_step(0)
    {
    }
    DeviceId GetId() const override { return m_id; }
    std::string GetName() const override { return m_name; }
    void Update() override
    {
        auto now = std::chrono::steady_clock::now();
        uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start).count();
        if(ms < 50) return;
        m_start = now;
        InputEvent e;
        if(m_step % 5 == 0) {
            e.type = InputEventType::KeyDown;
            e.key = 32 + (m_step % 10);
            e.pressed = true;
        } else if(m_step % 5 == 1) {
            e.type = InputEventType::KeyUp;
            e.key = 32 + ((m_step-1) % 10);
            e.pressed = false;
        } else if(m_step % 5 == 2) {
            e.type = InputEventType::MouseMove;
            e.mouseX = static_cast<float>((m_step % 400));
            e.mouseY = static_cast<float>(((m_step * 7) % 300));
        } else if(m_step % 5 == 3) {
            e.type = InputEventType::Axis;
            e.axisId = 0;
            e.axisValue = (float)((m_step % 2) ? 1.0f : -1.0f);
        } else {
            e.type = InputEventType::MouseButtonDown;
            e.mouseButton = 0;
            e.pressed = true;
        }
        e.device = m_id;
        e.timestamp = static_cast<Timestamp>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        if(m_callback) m_callback(e);
        ++m_step;
    }
    bool IsConnected() const override { return m_connected; }
    void SetEventCallback(EventCallback cb) override { m_callback = std::move(cb); }
private:
    DeviceId m_id;
    std::string m_name;
    std::atomic<bool> m_connected;
    EventCallback m_callback;
    std::chrono::steady_clock::time_point m_start;
    uint64_t m_step;
};

struct NetPacket
{
    std::vector<uint8_t> data;
    int deliverAtMs;
};

int main()
{
    InputManager& mgr = InputManager::Instance();
    auto dev = std::make_unique<SimulatedDevice>(1, "sim0");
    mgr.RegisterDevice(std::move(dev));

    NetworkInputLayer clientLayer(1001);
    NetworkInputLayer serverLayer(0);

    std::mutex outMutex;
    std::queue<NetPacket> networkQueue;
    std::default_random_engine rng((unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> jitter(30, 120);
    std::bernoulli_distribution drop(0.05);

    clientLayer.SetSendCallback([&](const NetworkInputLayer::Packet& p){
        if(p.empty()) return;
        if(drop(rng)) return;
        NetPacket pkt;
        pkt.data.assign(p.begin(), p.end());
        int now = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        pkt.deliverAtMs = now + jitter(rng);
        std::lock_guard<std::mutex> lk(outMutex);
        networkQueue.push(std::move(pkt));
    });

    auto makeCommandFromEvent = [&](const InputEvent& e)->InputCommand{
        InputCommand c{};
        c.seq = clientLayer.NextSeq();
        c.clientId = 1001;
        c.timestamp = static_cast<uint64_t>(e.timestamp);
        c.type = static_cast<uint16_t>(e.type);
        c.payload = {0,0,0,0};
        if(e.type == InputEventType::MouseMove) { c.payload[0] = e.mouseX; c.payload[1] = e.mouseY; }
        if(e.type == InputEventType::Axis) { c.payload[0] = e.axisValue; c.payload[1] = static_cast<float>(e.axisId); }
        if(e.type == InputEventType::KeyDown || e.type == InputEventType::KeyUp) { c.payload[0] = static_cast<float>(e.key); c.payload[1] = e.pressed ? 1.0f : 0.0f; }
        return c;
    };

    uint32_t lastAck = 0;

    auto subscriberId = mgr.Subscribe([&](const InputEvent& ev){
        auto cmd = makeCommandFromEvent(ev);
        clientLayer.EnqueueLocal(cmd);
    });

    auto serverValidate = [&](const InputCommand& c)->bool{
        if(c.type == static_cast<uint16_t>(InputEventType::KeyDown) || c.type == static_cast<uint16_t>(InputEventType::KeyUp)) return true;
        if(c.type == static_cast<uint16_t>(InputEventType::Axis)) {
            if(std::abs(c.payload[0]) <= 2.0f) return true;
            return false;
        }
        return true;
    };

    auto serverReconcile = [&](const InputCommand& c){
        std::cout << "[SERVER RECONCILE] client:" << c.clientId << " seq:" << c.seq << " type:" << c.type << " t:" << c.timestamp << "\n";
    };

    int loopMs = 0;
    for(int frame = 0; frame < 500; ++frame) {
        auto t0 = std::chrono::steady_clock::now();

        mgr.Update(0.016f);

        auto pkt = clientLayer.BuildOutgoingPacket(1200);
        if(!pkt.empty()) {
            // already enqueued by SetSendCallback lambda
        }

        {
            std::lock_guard<std::mutex> lk(outMutex);
            while(!networkQueue.empty()) {
                auto &np = networkQueue.front();
                int now = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
                if(now >= np.deliverAtMs) {
                    serverLayer.ReceiveIncomingPacket(np.data.data(), np.data.size());
                    networkQueue.pop();
                } else break;
            }
        }

        auto remoteCmds = serverLayer.ConsumeRemoteCommands();
        for(auto &cmd : remoteCmds) {
            if(serverValidate(cmd)) {
                serverReconcile(cmd);
                if(cmd.seq > lastAck) lastAck = cmd.seq;
            } else {
                std::cout << "[SERVER] validation failed seq:" << cmd.seq << "\n";
            }
        }

        if(lastAck) {
            clientLayer.ApplyAck(lastAck);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        auto t1 = std::chrono::steady_clock::now();
        loopMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
        if(frame % 60 == 0) {
            std::cout << "[DEBUG] frame " << frame << " loopMs=" << loopMs << " pendingNet=" << networkQueue.size() << " clientBuf=" << clientLayer.ConsumeRemoteCommands().size() << "\n";
        }
    }

    mgr.Unsubscribe(subscriberId);
    return 0;
}