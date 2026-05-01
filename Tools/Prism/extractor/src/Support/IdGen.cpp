/// @file Support/IdGen.cpp
/// @brief SHA-1-based stable symbol ID derivation using LLVM's hashing support.

#include "IdGen.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA1.h>

#include <iomanip>
#include <sstream>

namespace prism::support {

// ─── Internal ─────────────────────────────────────────────────────────────────

static std::string Sha1Hex16(const std::string& payload)
{
    llvm::SHA1 hasher;
    hasher.update(llvm::StringRef(payload.data(), payload.size()));

    const auto digest = hasher.final();

    std::ostringstream oss;
    // Use the first 8 bytes (16 hex chars) — collision probability is negligible
    // for symbol counts found in real codebases (< 10 M symbols).
    for (size_t i = 0; i < 8; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(static_cast<unsigned char>(digest[i]));

    return oss.str();
}

// ─── Public API ───────────────────────────────────────────────────────────────

std::string MakeSymbolId(const std::string& usr,
                          const std::string& fallback_file,
                          unsigned           fallback_line,
                          const std::string& fallback_kind)
{
    if (!usr.empty())
        return Sha1Hex16(usr);

    // Produce a deterministic fallback for declarations without a USR.
    // The null separator (\0) prevents accidental collisions between
    // e.g. "foo.cpp" + "12" + "Bar" and "foo.cpp1" + "2" + "Bar".
    const std::string composite =
        fallback_file + '\0' +
        std::to_string(fallback_line) + '\0' +
        fallback_kind;

    return Sha1Hex16(composite);
}

} // namespace prism::support
