#include "SagaEngine/Input/Network/InputBuffer.h"
using namespace Saga::Input::Network;

InputBuffer::InputBuffer(size_t capacity)
: cap(capacity)
{
}

void InputBuffer::Push(const InputCommand& c)
{
    std::lock_guard<std::mutex> lk(m);
    if(q.size() >= cap) q.pop_front();
    q.push_back(c);
}

std::vector<InputCommand> InputBuffer::GetSince(uint32_t seq)
{
    std::lock_guard<std::mutex> lk(m);
    std::vector<InputCommand> out;
    for(const auto &c : q) if(c.seq > seq) out.push_back(c);
    return out;
}

std::vector<InputCommand> InputBuffer::SnapshotAndClear()
{
    std::lock_guard<std::mutex> lk(m);
    std::vector<InputCommand> out(q.begin(), q.end());
    q.clear();
    return out;
}

void InputBuffer::AckUpTo(uint32_t seq)
{
    std::lock_guard<std::mutex> lk(m);
    while(!q.empty() && q.front().seq <= seq) q.pop_front();
    highestAcked = seq;
}

std::optional<InputCommand> InputBuffer::GetNextUnacked()
{
    std::lock_guard<std::mutex> lk(m);
    if(q.empty()) return std::nullopt;
    return q.front();
}

size_t InputBuffer::Size() const
{
    std::lock_guard<std::mutex> lk(m);
    return q.size();
}