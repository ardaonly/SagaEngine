/// @file Support/IdGen.h
/// @brief Deterministic stable symbol ID generation from Clang USR strings.
///
/// IDs are 16 hex characters derived from a SHA-1 digest.
/// The same USR always produces the same ID across machines and build runs.

#pragma once

#include <string>

namespace prism::support {

/// Produce a stable 16-character hex ID from *usr*.
///
/// When *usr* is empty (anonymous decl, macro-generated, or template arg),
/// falls back to a digest of *fallback_file* + *fallback_line* + *fallback_kind*
/// so every symbol still receives a unique, reproducible identifier.
std::string MakeSymbolId(const std::string& usr,
                          const std::string& fallback_file,
                          unsigned           fallback_line,
                          const std::string& fallback_kind);

} // namespace prism::support
