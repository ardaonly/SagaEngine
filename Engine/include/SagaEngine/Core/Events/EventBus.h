#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <any>

namespace SagaEngine::Core {

using EventId = uint32_t;
using SubscriberId = uint64_t;

struct EventContext {
    EventId eventId;
    SubscriberId subscriberId;
    bool handled = false;
};

template<typename T>
class Event {
public:
    using Handler = std::function<void(T&)>;

    explicit Event(EventId id) : _id(id) {}
    EventId GetId() const { return _id; }

    SubscriberId Subscribe(Handler handler);
    void Unsubscribe(SubscriberId id);
    void Publish(T& data);

private:
    EventId _id;
    mutable std::mutex _mutex;
    std::unordered_map<SubscriberId, Handler> _handlers;
    std::atomic<SubscriberId> _nextId{1};
};

template<typename T>
SubscriberId Event<T>::Subscribe(Handler handler) {
    std::lock_guard lock(_mutex);
    SubscriberId id = _nextId.fetch_add(1, std::memory_order_relaxed);
    _handlers[id] = std::move(handler);
    return id;
}

template<typename T>
void Event<T>::Unsubscribe(SubscriberId id) {
    std::lock_guard lock(_mutex);
    _handlers.erase(id);
}

template<typename T>
void Event<T>::Publish(T& data) {
    std::lock_guard lock(_mutex);
    for (auto& [id, handler] : _handlers) {
        handler(data);
    }
}

class EventBus {
public:
    static EventBus& Instance();

    template<typename T>
    Event<T>& GetEvent(EventId id);

    template<typename T>
    SubscriberId Subscribe(EventId id, typename Event<T>::Handler handler);

    template<typename T>
    void Publish(EventId id, T& data);

private:
    EventBus() = default;
    std::mutex _mutex;
    std::unordered_map<EventId, std::any> _events;
};

template<typename T>
Event<T>& EventBus::GetEvent(EventId id) {
    std::lock_guard lock(_mutex);
    auto it = _events.find(id);
    if (it == _events.end()) {
        _events[id] = Event<T>(id);
    }
    return std::any_cast<Event<T>&>(_events[id]);
}

template<typename T>
SubscriberId EventBus::Subscribe(EventId id, typename Event<T>::Handler handler) {
    return GetEvent<T>(id).Subscribe(std::move(handler));
}

template<typename T>
void EventBus::Publish(EventId id, T& data) {
    GetEvent<T>(id).Publish(data);
}

} // namespace SagaEngine::Core