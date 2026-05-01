/// @file Encoding.cpp
/// @brief UTF-8 ↔ UTF-16 conversion via Win32 MultiByteToWideChar (Windows)
///        and pass-through getenv on POSIX.

#include "SagaTools/Encoding.h"

#include <cstdlib>

#if defined(_WIN32)
    #include <windows.h>
#endif

namespace SagaTools
{

#if defined(_WIN32)

// ─── UTF-8 ↔ UTF-16 ──────────────────────────────────────────────────────────

std::wstring Utf8ToWide(const std::string& utf8)
{
    if (utf8.empty()) return {};
    const int needed = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS,
        utf8.data(), static_cast<int>(utf8.size()),
        nullptr, 0);
    if (needed <= 0) return {};
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS,
        utf8.data(), static_cast<int>(utf8.size()),
        out.data(), needed);
    return out;
}

std::string WideToUtf8(const std::wstring& wide)
{
    if (wide.empty()) return {};
    const int needed = WideCharToMultiByte(
        CP_UTF8, 0,
        wide.data(), static_cast<int>(wide.size()),
        nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0,
        wide.data(), static_cast<int>(wide.size()),
        out.data(), needed, nullptr, nullptr);
    return out;
}

// ─── Environment Lookup (Windows) ────────────────────────────────────────────

std::string GetEnvUtf8(const std::string& name) noexcept
{
    // Use the wide environment so non-ASCII values survive.
    const std::wstring wname = Utf8ToWide(name);
    if (wname.empty()) return {};
    const wchar_t* wval = _wgetenv(wname.c_str());
    if (!wval || !*wval) return {};
    try {
        return WideToUtf8(wval);
    } catch (...) {
        return {};
    }
}

#else

// ─── Environment Lookup (POSIX) ──────────────────────────────────────────────

std::string GetEnvUtf8(const std::string& name) noexcept
{
    if (const char* v = std::getenv(name.c_str()); v && *v) return v;
    return {};
}

#endif

} // namespace SagaTools
