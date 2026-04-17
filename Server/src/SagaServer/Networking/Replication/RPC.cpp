/// @file RPC.cpp
/// @brief RPC codec and dispatch table implementation.
///
/// Layer  : SagaServer / Networking / Replication
/// Purpose: Production implementation of the RPC system with TLV encoding,
///          rate limiting, and dispatch table.

#include "SagaServer/Networking/Replication/RPC.h"
#include "SagaEngine/Core/Log/Log.h"

#include <chrono>
#include <cstring>

namespace SagaServer {

using namespace std::chrono;

static constexpr const char* kTag = "RPC";

// ─── Wire format helpers ────────────────────────────────────────────────

namespace {

void WriteU16(uint8_t* dst, uint16_t value) noexcept
{
    dst[0] = static_cast<uint8_t>(value & 0xFF);
    dst[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void WriteU32(uint8_t* dst, uint32_t value) noexcept
{
    dst[0] = static_cast<uint8_t>(value & 0xFF);
    dst[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    dst[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    dst[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

uint16_t ReadU16(const uint8_t* src) noexcept
{
    return static_cast<uint16_t>(src[0]) | (static_cast<uint16_t>(src[1]) << 8);
}

uint32_t ReadU32(const uint8_t* src) noexcept
{
    return static_cast<uint32_t>(src[0]) |
           (static_cast<uint32_t>(src[1]) << 8) |
           (static_cast<uint32_t>(src[2]) << 16) |
           (static_cast<uint32_t>(src[3]) << 24);
}

} // anonymous namespace

// ─── Codec: Encode request ──────────────────────────────────────────────

bool RPCCodec::EncodeRequest(const RPCRequest& request,
                              std::vector<uint8_t>& outBuffer) noexcept
{
    outBuffer.clear();

    // Validate name length.
    if (request.rpcName.empty() || request.rpcName.size() > kMaxRPCNameLength)
        return false;

    // Validate argument count.
    if (request.arguments.size() > kMaxRPCArguments)
        return false;

    // Calculate total size: nameLength(2) + name(N) + argCount(1) + args...
    std::size_t totalSize = 2 + request.rpcName.size() + 1;
    for (const auto& arg : request.arguments)
    {
        totalSize += 1;  // typeTag
        totalSize += 2;  // length
        totalSize += arg.data.size();
    }

    if (totalSize > kMaxRPCPayloadBytes)
        return false;

    outBuffer.resize(totalSize);
    uint8_t* ptr = outBuffer.data();

    // Write name length + name.
    const uint16_t nameLen = static_cast<uint16_t>(request.rpcName.size());
    WriteU16(ptr, nameLen);
    ptr += 2;
    std::memcpy(ptr, request.rpcName.data(), nameLen);
    ptr += nameLen;

    // Write argument count.
    ptr[0] = static_cast<uint8_t>(request.arguments.size());
    ptr += 1;

    // Write each argument (TLV: type-length-value).
    for (const auto& arg : request.arguments)
    {
        if (!WriteArgument(outBuffer, arg))
        {
            outBuffer.clear();
            return false;
        }
    }

    return true;
}

bool RPCCodec::WriteArgument(std::vector<uint8_t>& buffer, const RPCArgument& arg) noexcept
{
    // Find the end of the current buffer to append.
    std::size_t currentSize = buffer.size();

    // Calculate argument size: type(1) + length(2) + data(N).
    std::size_t argSize = 1 + 2 + arg.data.size();

    // Resize buffer to accommodate.
    buffer.resize(currentSize + argSize);
    uint8_t* ptr = buffer.data() + currentSize;

    // Write type tag.
    ptr[0] = static_cast<uint8_t>(arg.type);
    ptr += 1;

    // Write data length.
    WriteU16(ptr, static_cast<uint16_t>(arg.data.size()));
    ptr += 2;

    // Write data.
    if (!arg.data.empty())
    {
        std::memcpy(ptr, arg.data.data(), arg.data.size());
    }

    return true;
}

// ─── Codec: Decode request ──────────────────────────────────────────────

bool RPCCodec::DecodeRequest(const uint8_t* data, std::size_t size,
                              uint64_t clientId, uint32_t rpcId,
                              RPCRequest& outRequest) noexcept
{
    if (!data || size < 3)  // Minimum: nameLength(2) + argCount(1)
        return false;

    std::size_t offset = 0;

    // Read name length.
    if (size < offset + 2)
        return false;
    const uint16_t nameLen = ReadU16(data + offset);
    offset += 2;

    if (nameLen == 0 || nameLen > kMaxRPCNameLength)
        return false;
    if (size < offset + nameLen)
        return false;

    outRequest.rpcName.assign(reinterpret_cast<const char*>(data + offset), nameLen);
    offset += nameLen;

    // Read argument count.
    if (size < offset + 1)
        return false;
    const uint8_t argCount = data[offset];
    offset += 1;

    if (argCount > kMaxRPCArguments)
        return false;

    outRequest.clientId = clientId;
    outRequest.rpcId = rpcId;
    outRequest.arguments.clear();
    outRequest.arguments.reserve(argCount);

    // Read each argument.
    for (uint8_t i = 0; i < argCount; ++i)
    {
        RPCArgument arg;
        if (!ReadArgument(data, size, offset, arg))
        {
            outRequest.arguments.clear();
            return false;
        }
        outRequest.arguments.push_back(std::move(arg));
    }

    return true;
}

bool RPCCodec::ReadArgument(const uint8_t* data, std::size_t size, std::size_t& offset,
                             RPCArgument& outArg) noexcept
{
    // Read type tag.
    if (size < offset + 1)
        return false;
    outArg.type = static_cast<RPCArgType>(data[offset]);
    offset += 1;

    // Read data length.
    if (size < offset + 2)
        return false;
    const uint16_t dataLen = ReadU16(data + offset);
    offset += 2;

    // Validate data length against type.
    const uint16_t expectedSize = RPCArgTypeSize(outArg.type);
    if (expectedSize > 0 && dataLen != expectedSize)
        return false;  // Fixed-size type mismatch.

    if (size < offset + dataLen)
        return false;

    // Read data.
    outArg.data.assign(data + offset, data + offset + dataLen);
    offset += dataLen;

    return true;
}

// ─── Codec: Encode response ─────────────────────────────────────────────

bool RPCCodec::EncodeResponse(const RPCResponse& response,
                               std::vector<uint8_t>& outBuffer) noexcept
{
    outBuffer.clear();

    // Size: rpcId(4) + statusCode(1) + resultData(N).
    const std::size_t totalSize = 4 + 1 + response.resultData.size();
    if (totalSize > kMaxRPCPayloadBytes)
        return false;

    outBuffer.resize(totalSize);
    uint8_t* ptr = outBuffer.data();

    WriteU32(ptr, response.rpcId);
    ptr += 4;

    ptr[0] = static_cast<uint8_t>(response.status);
    ptr += 1;

    if (!response.resultData.empty())
    {
        std::memcpy(ptr, response.resultData.data(), response.resultData.size());
    }

    return true;
}

// ─── Codec: Decode response ─────────────────────────────────────────────

bool RPCCodec::DecodeResponse(const uint8_t* data, std::size_t size,
                               RPCResponse& outResponse) noexcept
{
    if (!data || size < 5)  // Minimum: rpcId(4) + statusCode(1)
        return false;

    outResponse.rpcId = ReadU32(data);
    outResponse.status = static_cast<RPCStatusCode>(data[4]);

    const std::size_t resultOffset = 5;
    if (size > resultOffset)
    {
        outResponse.resultData.assign(data + resultOffset, data + size);
    }
    else
    {
        outResponse.resultData.clear();
    }

    return true;
}

// ─── Dispatch: Register ─────────────────────────────────────────────────

bool RPCDispatch::Register(const char* name, RPCHandlerFn handler,
                            bool requiresAuth, uint32_t cooldownMs) noexcept
{
    if (!name || !handler)
        return false;

    const std::string nameStr(name);
    if (nameStr.empty() || nameStr.size() > kMaxRPCNameLength)
    {
        LOG_WARN(kTag, "RPC name too long or empty: '%s'", name);
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_handlers.count(nameStr) != 0)
    {
        LOG_WARN(kTag, "RPC '%s' already registered; overwriting", name);
    }

    RPCEntry entry;
    entry.name = nameStr;
    entry.handler = std::move(handler);
    entry.requiresAuth = requiresAuth;
    entry.cooldownMs = cooldownMs;

    m_handlers.emplace(nameStr, std::move(entry));

    LOG_DEBUG(kTag, "Registered RPC: '%s' (auth: %s, cooldown: %u ms)",
              name, requiresAuth ? "yes" : "no", cooldownMs);

    return true;
}

bool RPCDispatch::Unregister(const char* name) noexcept
{
    if (!name)
        return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    return m_handlers.erase(name) > 0;
}

// ─── Dispatch: Dispatch ─────────────────────────────────────────────────

bool RPCDispatch::Dispatch(uint64_t clientId, bool clientAuth,
                            const RPCRequest& request, RPCResponse& outResponse) noexcept
{
    outResponse.rpcId = request.rpcId;
    outResponse.status = RPCStatusCode::InternalError;

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_handlers.find(request.rpcName);
    if (it == m_handlers.end())
    {
        outResponse.status = RPCStatusCode::NotFound;
        LOG_WARN(kTag, "RPC '%s' not found (client %llu)",
                 request.rpcName.c_str(), static_cast<unsigned long long>(clientId));
        return false;
    }

    const RPCEntry& entry = it->second;

    // Check authentication.
    if (entry.requiresAuth && !clientAuth)
    {
        outResponse.status = RPCStatusCode::AuthFailed;
        LOG_WARN(kTag, "RPC '%s' denied: client %llu not authenticated",
                 request.rpcName.c_str(), static_cast<unsigned long long>(clientId));
        return true;  // Handler was found; auth gate blocked it.
    }

    // Check rate limit.
    {
        std::lock_guard<std::mutex> rateLock(m_rateMutex);
        auto& clientRates = m_rates[clientId];
        auto& rateLimiter = clientRates[request.rpcName];
        rateLimiter.cooldownMs = entry.cooldownMs;

        const uint64_t nowUs = static_cast<uint64_t>(
            duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());

        if (!rateLimiter.AllowCall(nowUs))
        {
            outResponse.status = RPCStatusCode::RateLimited;
            return true;  // Handler found; rate limited.
        }

        rateLimiter.RecordCall(nowUs);
    }

    // Call the handler.
    const bool success = entry.handler(clientId, request, outResponse);
    if (!success)
    {
        outResponse.status = RPCStatusCode::InternalError;
        LOG_WARN(kTag, "RPC '%s' handler failed for client %llu",
                 request.rpcName.c_str(), static_cast<unsigned long long>(clientId));
    }
    else
    {
        outResponse.status = RPCStatusCode::Ok;
    }

    return true;
}

uint32_t RPCDispatch::Count() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<uint32_t>(m_handlers.size());
}

std::vector<std::string> RPCDispatch::GetNames() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_handlers.size());

    for (const auto& [name, _] : m_handlers)
        names.push_back(name);

    return names;
}

} // namespace SagaServer
