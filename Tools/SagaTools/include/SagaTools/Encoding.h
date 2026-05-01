/// @file Encoding.h
/// @brief UTF-8 ↔ UTF-16 helpers and UTF-8-aware filesystem path construction.

#pragma once

#include <filesystem>
#include <string>

#if defined(_WIN32)
    #include <wchar.h>
#endif

namespace SagaTools
{

// ─── Encoding helpers ─────────────────────────────────────────────────────────

/// Convert a UTF-8 string to a `std::wstring` (UTF-16 on Windows).
///
/// On POSIX, `wchar_t` is 32-bit and the wide variants of the platform APIs
/// are not used by SagaTools, so this helper is intentionally Windows-only.
/// Calling it on POSIX is a programmer error and is allowed to fail to link;
/// every call site is gated with `#if defined(_WIN32)`.
#if defined(_WIN32)
[[nodiscard]] std::wstring Utf8ToWide(const std::string& utf8);
[[nodiscard]] std::string  WideToUtf8(const std::wstring& wide);
#endif

// ─── Path construction ───────────────────────────────────────────────────────

/// Build a `std::filesystem::path` from a UTF-8 byte string.
///
/// Why not just `fs::path(utf8)`? On Windows the `fs::path(const std::string&)`
/// constructor interprets the input bytes through the *active ANSI code page*
/// (CP1254 on Turkish Windows, CP1252 on Western, …) — UTF-8 bytes get
/// silently mojibake'd into something Windows cannot find on disk. The
/// `u8path` constructor explicitly says "these bytes are UTF-8" and the path
/// gets stored as the correct UTF-16 sequence internally.
///
/// On POSIX every API is byte-transparent, so this helper just forwards.
[[nodiscard]] inline std::filesystem::path PathFromUtf8(const std::string& utf8)
{
#if defined(_WIN32)
    // u8path is technically deprecated in C++20 in favour of the char8_t
    // overload, but the deprecation does not change the runtime behaviour
    // and char8_t is awkward to feed from a std::string. Suppress the
    // deprecation locally; this is the right call here.
    #if defined(__clang__) || defined(__GNUC__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    #elif defined(_MSC_VER)
        #pragma warning(push)
        #pragma warning(disable: 4996)
    #endif
    return std::filesystem::u8path(utf8);
    #if defined(__clang__) || defined(__GNUC__)
        #pragma GCC diagnostic pop
    #elif defined(_MSC_VER)
        #pragma warning(pop)
    #endif
#else
    return std::filesystem::path(utf8);
#endif
}

/// Render a `std::filesystem::path` back as a UTF-8 byte string. Mirrors
/// `PathFromUtf8` so paths can round-trip through the dispatcher's logic
/// without ever touching the platform's narrow code page.
[[nodiscard]] inline std::string PathToUtf8(const std::filesystem::path& p)
{
#if defined(_WIN32)
    const auto u8 = p.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
#else
    return p.string();
#endif
}

// ─── Environment lookup ──────────────────────────────────────────────────────

/// UTF-8 environment-variable lookup. Returns an empty string when unset.
///
/// On Windows the standard `getenv` returns ANSI bytes. The wide variant
/// `_wgetenv` returns the real UTF-16 value, which we convert back to UTF-8
/// so the rest of the dispatcher can stay UTF-8-only.
[[nodiscard]] std::string GetEnvUtf8(const std::string& name) noexcept;

} // namespace SagaTools
