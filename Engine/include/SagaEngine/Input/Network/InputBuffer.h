#pragma once
#include "InputCommand.h"
#include <deque>
#include <vector>
#include <mutex>
#include <optional>

namespace Saga::Input::Network
{
class InputBuffer
{
public:
    InputBuffer(size_t capacity = 4096);
    void Push(const InputCommand& c);
    std::vector<InputCommand> GetSince(uint32_t seq);
    std::vector<InputCommand> SnapshotAndClear();
    void AckUpTo(uint32_t seq);
    std::optional<InputCommand> GetNextUnacked();
    size_t Size() const;
private:
    mutable std::mutex m;
    std::deque<InputCommand> q;
    size_t cap;
    uint32_t highestAcked = 0;
};
}