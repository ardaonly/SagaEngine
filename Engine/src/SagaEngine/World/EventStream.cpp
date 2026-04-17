/// @file EventStream.cpp
/// @brief Append-only event log implementation with disk persistence.

#include "SagaEngine/World\EventStream.h"
#include "SagaEngine/Core/Log/Log.h"

#include <algorithm>
#include <cstring>

#if defined(_WIN32)
    #include <windows.h>
    #include <fileapi.h>
#else
    #include <cstdio>
    #include <unistd.h>
    #include <fcntl.h>
#endif

namespace SagaEngine::World {

static constexpr const char* kTag = "EventStream";

// ─── Binary format constants ──────────────────────────────────────────────────
//
// Event file format (append-only):
//   [Header: 40 bytes]
//     sequence(8) | worldTick(8) | timestampUs(8) | type(2) |
//     cellX(2) | cellZ(2) | entityId(4) | data(4) | payloadLen(4)
//   [Payload: payloadLen bytes]

static constexpr size_t kEventHeaderSize = 40;

// ─── In-memory event ops ──────────────────────────────────────────────────────

void EventStream::Append(WorldEvent evt) noexcept
{
    m_lastSequence++;
    evt.sequence = m_lastSequence;

    // Buffer to disk if file is open.
    if (m_fileHandle)
    {
        auto serialized = SerializeEvent(evt);
        m_writeBuffer.insert(m_writeBuffer.end(),
                              serialized.begin(), serialized.end());

        if (m_writeBuffer.size() >= kFlushThreshold)
            FlushPending();
    }

    m_events.push_back(std::move(evt));
}

void EventStream::AppendBatch(const std::vector<WorldEvent>& events) noexcept
{
    m_events.reserve(m_events.size() + events.size());
    for (const auto& evt : events)
    {
        m_lastSequence++;
        WorldEvent e = evt;
        e.sequence = m_lastSequence;

        // Buffer to disk if file is open.
        if (m_fileHandle)
        {
            auto serialized = SerializeEvent(e);
            m_writeBuffer.insert(m_writeBuffer.end(),
                                  serialized.begin(), serialized.end());
        }

        m_events.push_back(std::move(e));
    }

    if (m_writeBuffer.size() >= kFlushThreshold)
        FlushPending();
}

void EventStream::TakeSnapshot(WorldSnapshot snap) noexcept
{
    snap.baseSequence = m_lastSequence;
    m_snapshots.push_back(std::move(snap));
}

const WorldSnapshot* EventStream::LatestSnapshot() const noexcept
{
    if (m_snapshots.empty())
        return nullptr;
    return &m_snapshots.back();
}

void EventStream::ReplayFromStart(EventHandlerFn handler) const noexcept
{
    for (const auto& evt : m_events)
        handler(evt);
}

void EventStream::ReplayFromSnapshot(EventHandlerFn handler) const noexcept
{
    const auto* snap = LatestSnapshot();
    if (!snap)
    {
        ReplayFromStart(std::move(handler));
        return;
    }

    // Replay events after the snapshot's base sequence.
    ReplayRange(snap->baseSequence + 1, m_lastSequence, std::move(handler));
}

void EventStream::ReplayRange(uint64_t fromSeq, uint64_t toSeq,
                                EventHandlerFn handler) const noexcept
{
    for (const auto& evt : m_events)
    {
        if (evt.sequence < fromSeq)
            continue;
        if (evt.sequence > toSeq)
            break;
        handler(evt);
    }
}

const WorldEvent* EventStream::GetEvent(uint64_t seq) const noexcept
{
    if (seq == 0 || seq > m_lastSequence)
        return nullptr;

    // Binary search would be better for large streams, but linear scan
    // is fine for < 1M events.  Production should use an index.
    for (const auto& evt : m_events)
    {
        if (evt.sequence == seq)
            return &evt;
    }
    return nullptr;
}

std::vector<WorldEvent> EventStream::GetEventsAfter(uint64_t lastSeenSeq) const noexcept
{
    std::vector<WorldEvent> out;
    out.reserve(m_events.size()); // upper bound

    bool found = false;
    for (const auto& evt : m_events)
    {
        if (!found)
        {
            if (evt.sequence == lastSeenSeq)
                found = true;
            continue;
        }
        out.push_back(evt);
    }

    return out;
}

// ─── Disk persistence ─────────────────────────────────────────────────────────

bool EventStream::Open(const char* path) noexcept
{
    if (!path || path[0] == '\0')
        return false;

    std::strncpy(m_filePath, path, sizeof(m_filePath) - 1);
    m_filePath[sizeof(m_filePath) - 1] = '\0';

#if defined(_WIN32)
    HANDLE h = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (h == INVALID_HANDLE_VALUE)
    {
        LOG_ERROR(kTag, "Failed to open event log: %s", path);
        return false;
    }
    m_fileHandle = h;
#else
    FILE* f = std::fopen(path, "ab");
    if (!f)
    {
        LOG_ERROR(kTag, "Failed to open event log: %s", path);
        return false;
    }
    m_fileHandle = f;
#endif

    LOG_INFO(kTag, "Event log opened: %s", path);
    return true;
}

void EventStream::Close() noexcept
{
    FlushPending();

#if defined(_WIN32)
    if (m_fileHandle && m_fileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(static_cast<HANDLE>(m_fileHandle));
        m_fileHandle = nullptr;
    }
#else
    if (m_fileHandle)
    {
        std::fclose(static_cast<FILE*>(m_fileHandle));
        m_fileHandle = nullptr;
    }
#endif

    m_filePath[0] = '\0';
}

void EventStream::FlushPending() noexcept
{
    if (m_writeBuffer.empty() || !m_fileHandle)
        return;

#if defined(_WIN32)
    HANDLE h = static_cast<HANDLE>(m_fileHandle);
    DWORD written = 0;
    BOOL ok = WriteFile(h, m_writeBuffer.data(),
                        static_cast<DWORD>(m_writeBuffer.size()), &written, nullptr);
    if (!ok || written != m_writeBuffer.size())
    {
        LOG_ERROR(kTag, "Failed to write event buffer to disk.");
        return;
    }

    // fsync equivalent on Windows.
    FlushFileBuffers(h);
#else
    FILE* f = static_cast<FILE*>(m_fileHandle);
    size_t written = std::fwrite(m_writeBuffer.data(), 1,
                                  m_writeBuffer.size(), f);
    if (written != m_writeBuffer.size())
    {
        LOG_ERROR(kTag, "Failed to write event buffer to disk.");
        return;
    }

    std::fflush(f);
#if defined(_POSIX_VERSION)
    fsync(fileno(f));
#endif
#endif

    m_writeBuffer.clear();
}

bool EventStream::LoadFromFile() noexcept
{
    if (!m_filePath[0])
        return false;

#if defined(_WIN32)
    // Open for reading.
    HANDLE h = CreateFileA(
        m_filePath,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (h == INVALID_HANDLE_VALUE)
        return false; // File doesn't exist yet — not an error.

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(h, &fileSize))
    {
        CloseHandle(h);
        return false;
    }

    if (fileSize.QuadPart == 0)
    {
        CloseHandle(h);
        return true; // Empty file.
    }

    // Read entire file.
    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize.QuadPart));
    DWORD read = 0;
    BOOL ok = ReadFile(h, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr);
    CloseHandle(h);

    if (!ok || read == 0)
        return false;
#else
    FILE* f = std::fopen(m_filePath, "rb");
    if (!f)
        return false;

    std::fseek(f, 0, SEEK_END);
    const long fileSize = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    if (fileSize <= 0)
    {
        std::fclose(f);
        return true;
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    const size_t read = std::fread(buffer.data(), 1, buffer.size(), f);
    std::fclose(f);

    if (read == 0)
        return false;
#endif

    // Parse events from buffer.
    size_t offset = 0;
    while (offset + kEventHeaderSize <= buffer.size())
    {
        auto evtOpt = DeserializeEvent(buffer.data() + offset,
                                        buffer.size() - offset);
        if (!evtOpt)
            break; // Malformed or end of data.

        WorldEvent evt = std::move(*evtOpt);
        m_events.push_back(std::move(evt));

        if (m_events.back().sequence > m_lastSequence)
            m_lastSequence = m_events.back().sequence;

        // Advance past header + payload.
        const uint32_t payloadLen = *reinterpret_cast<const uint32_t*>(
            buffer.data() + offset + kEventHeaderSize - 4);
        offset += kEventHeaderSize + payloadLen;
    }

    LOG_INFO(kTag, "Loaded %zu events from disk (last seq: %llu).",
             m_events.size(), static_cast<unsigned long long>(m_lastSequence));
    return true;
}

bool EventStream::SaveSnapshot(const WorldSnapshot& snap) noexcept
{
    if (snap.stateData.empty())
        return false;

    // Snapshot format: [baseSequence(8) | worldTick(8) | timestampUs(8) |
    //                   dataLen(4)] [data: dataLen bytes]
    static constexpr size_t kSnapHeaderSize = 28;
    std::vector<uint8_t> buffer(kSnapHeaderSize + snap.stateData.size());

    size_t off = 0;
    std::memcpy(buffer.data() + off, &snap.baseSequence, 8); off += 8;
    std::memcpy(buffer.data() + off, &snap.worldTick, 8);    off += 8;
    std::memcpy(buffer.data() + off, &snap.timestampUs, 8);  off += 8;
    uint32_t dataLen = static_cast<uint32_t>(snap.stateData.size());
    std::memcpy(buffer.data() + off, &dataLen, 4);           off += 4;
    std::memcpy(buffer.data() + off, snap.stateData.data(), snap.stateData.size());

    const char* snapPath = "snapshot.bin"; // TODO: use config.

#if defined(_WIN32)
    HANDLE h = CreateFileA(snapPath, GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    DWORD written = 0;
    BOOL ok = WriteFile(h, buffer.data(), static_cast<DWORD>(buffer.size()), &written, nullptr);
    FlushFileBuffers(h);
    CloseHandle(h);
    return ok && written == buffer.size();
#else
    FILE* f = std::fopen(snapPath, "wb");
    if (!f)
        return false;
    size_t written = std::fwrite(buffer.data(), 1, buffer.size(), f);
    std::fclose(f);
    return written == buffer.size();
#endif
}

std::optional<WorldSnapshot> EventStream::LoadLatestSnapshot() const noexcept
{
    const char* snapPath = "snapshot.bin";

#if defined(_WIN32)
    HANDLE h = CreateFileA(snapPath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return std::nullopt;

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(h, &fileSize) || fileSize.QuadPart < 28)
    {
        CloseHandle(h);
        return std::nullopt;
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize.QuadPart));
    DWORD read = 0;
    BOOL ok = ReadFile(h, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr);
    CloseHandle(h);
    if (!ok || read < 28)
        return std::nullopt;
#else
    FILE* f = std::fopen(snapPath, "rb");
    if (!f)
        return std::nullopt;

    std::fseek(f, 0, SEEK_END);
    const long fileSize = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    if (fileSize < 28)
    {
        std::fclose(f);
        return std::nullopt;
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    const size_t rd = std::fread(buffer.data(), 1, buffer.size(), f);
    std::fclose(f);
    if (rd < 28)
        return std::nullopt;
#endif

    WorldSnapshot snap;
    size_t off = 0;
    std::memcpy(&snap.baseSequence, buffer.data() + off, 8); off += 8;
    std::memcpy(&snap.worldTick,    buffer.data() + off, 8); off += 8;
    std::memcpy(&snap.timestampUs,  buffer.data() + off, 8); off += 8;
    uint32_t dataLen = 0;
    std::memcpy(&dataLen, buffer.data() + off, 4);           off += 4;

    if (off + dataLen > buffer.size())
        return std::nullopt;

    snap.stateData.assign(buffer.data() + off, buffer.data() + off + dataLen);
    return snap;
}

// ─── Serialization helpers ────────────────────────────────────────────────────

std::vector<uint8_t> EventStream::SerializeEvent(const WorldEvent& evt) const noexcept
{
    std::vector<uint8_t> out(kEventHeaderSize + evt.payload.size());
    size_t off = 0;

    std::memcpy(out.data() + off, &evt.sequence, 8);    off += 8;
    std::memcpy(out.data() + off, &evt.worldTick, 8);   off += 8;
    std::memcpy(out.data() + off, &evt.timestampUs, 8); off += 8;

    uint16_t typeVal = static_cast<uint16_t>(evt.type);
    std::memcpy(out.data() + off, &typeVal, 2);         off += 2;

    int16_t cx = evt.cellId.worldX;
    int16_t cz = evt.cellId.worldZ;
    std::memcpy(out.data() + off, &cx, 2);              off += 2;
    std::memcpy(out.data() + off, &cz, 2);              off += 2;

    std::memcpy(out.data() + off, &evt.entityId, 4);    off += 4;
    std::memcpy(out.data() + off, &evt.data, 4);        off += 4;

    uint32_t payloadLen = static_cast<uint32_t>(evt.payload.size());
    std::memcpy(out.data() + off, &payloadLen, 4);      off += 4;

    if (!evt.payload.empty())
    {
        std::memcpy(out.data() + off, evt.payload.data(), evt.payload.size());
    }

    return out;
}

std::optional<WorldEvent> EventStream::DeserializeEvent(
    const uint8_t* data, size_t size) noexcept
{
    if (size < kEventHeaderSize)
        return std::nullopt;

    WorldEvent evt;
    size_t off = 0;

    std::memcpy(&evt.sequence,    data + off, 8); off += 8;
    std::memcpy(&evt.worldTick,   data + off, 8); off += 8;
    std::memcpy(&evt.timestampUs, data + off, 8); off += 8;

    uint16_t typeVal = 0;
    std::memcpy(&typeVal, data + off, 2); off += 2;
    evt.type = static_cast<WorldEventType>(typeVal);

    int16_t cx = 0, cz = 0;
    std::memcpy(&cx, data + off, 2); off += 2;
    std::memcpy(&cz, data + off, 2); off += 2;
    evt.cellId.worldX = cx;
    evt.cellId.worldZ = cz;
    evt.cellId.gen = 0;

    std::memcpy(&evt.entityId, data + off, 4); off += 4;
    std::memcpy(&evt.data,     data + off, 4); off += 4;

    uint32_t payloadLen = 0;
    std::memcpy(&payloadLen, data + off, 4); off += 4;

    if (size < off + payloadLen)
        return std::nullopt; // Incomplete payload.

    if (payloadLen > 0)
    {
        evt.payload.assign(data + off, data + off + payloadLen);
    }

    return evt;
}

} // namespace SagaEngine::World
