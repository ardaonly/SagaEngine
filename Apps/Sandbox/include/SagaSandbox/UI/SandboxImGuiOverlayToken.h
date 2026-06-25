/// @file SandboxImGuiOverlayToken.h
/// @brief Internal helpers for opaque ImGui overlay texture tokens.

#pragma once

#include <cstdint>
#include <type_traits>

namespace SagaSandbox::Detail
{

constexpr std::uint32_t kOverlayTokenInvalid = 0u;

[[nodiscard]] constexpr std::uint32_t OverlayTokenMaxId() noexcept
{
    if constexpr (sizeof(std::uintptr_t) >= 8u)
    {
        return 0xFFFFu;
    }
    else
    {
        return 0x7FFFu;
    }
}

[[nodiscard]] constexpr std::uint32_t AdvanceOverlayTokenGeneration(
    std::uint32_t generation) noexcept
{
    ++generation;
    if (generation == 0u)
    {
        generation = 1u;
    }
    return generation;
}

[[nodiscard]] constexpr std::uintptr_t PackOverlayToken(
    std::uint32_t magic,
    std::uint32_t generation,
    std::uint32_t id) noexcept
{
    if (id == 0u || id > OverlayTokenMaxId() || generation == 0u ||
        magic == 0u)
    {
        return 0u;
    }

    if constexpr (sizeof(std::uintptr_t) >= 8u)
    {
        return (static_cast<std::uintptr_t>(magic & 0xFFFFu) << 32u) |
               (static_cast<std::uintptr_t>(generation & 0xFFFFu) << 16u) |
               (static_cast<std::uintptr_t>(id) & 0xFFFFu);
    }
    else
    {
        return (static_cast<std::uintptr_t>(1u) << 31u) |
               (static_cast<std::uintptr_t>(magic & 0xFFu) << 23u) |
               (static_cast<std::uintptr_t>(generation & 0xFFu) << 15u) |
               (static_cast<std::uintptr_t>(id) & 0x7FFFu);
    }
}

[[nodiscard]] constexpr std::uint32_t OverlayTokenMagic(
    std::uintptr_t token) noexcept
{
    if constexpr (sizeof(std::uintptr_t) >= 8u)
    {
        return static_cast<std::uint32_t>((token >> 32u) & 0xFFFFu);
    }
    else
    {
        return static_cast<std::uint32_t>((token >> 23u) & 0xFFu);
    }
}

[[nodiscard]] constexpr std::uint32_t OverlayTokenGeneration(
    std::uintptr_t token) noexcept
{
    if constexpr (sizeof(std::uintptr_t) >= 8u)
    {
        return static_cast<std::uint32_t>((token >> 16u) & 0xFFFFu);
    }
    else
    {
        return static_cast<std::uint32_t>((token >> 15u) & 0xFFu);
    }
}

[[nodiscard]] constexpr std::uint32_t OverlayTokenId(
    std::uintptr_t token) noexcept
{
    if constexpr (sizeof(std::uintptr_t) >= 8u)
    {
        return static_cast<std::uint32_t>(token & 0xFFFFu);
    }
    else
    {
        return static_cast<std::uint32_t>(token & 0x7FFFu);
    }
}

template <typename ImTextureIdT>
[[nodiscard]] ImTextureIdT OverlayTokenToImTextureID(
    std::uintptr_t token) noexcept
{
    if constexpr (std::is_pointer_v<ImTextureIdT>)
    {
        return reinterpret_cast<ImTextureIdT>(token);
    }
    else
    {
        return static_cast<ImTextureIdT>(token);
    }
}

template <typename ImTextureIdT>
[[nodiscard]] std::uintptr_t ImTextureIDToOverlayToken(
    ImTextureIdT texture) noexcept
{
    if constexpr (std::is_pointer_v<ImTextureIdT>)
    {
        return reinterpret_cast<std::uintptr_t>(texture);
    }
    else
    {
        return static_cast<std::uintptr_t>(texture);
    }
}

} // namespace SagaSandbox::Detail
