/// @file Support/PathUtils.h
/// @brief Source-path normalization and repository-relative path derivation.
///
/// All functions in this file are pure and thread-safe.
/// They operate on std::string to remain independent of LLVM's filesystem APIs.

#pragma once

#include <string>

namespace prism::support {

/// Convert *abs_path* to a POSIX-style path relative to *repo_root*.
/// Returns *abs_path* unchanged when relativization is not possible
/// (e.g. the path lives outside the repository tree).
std::string MakeRelativePosix(const std::string& abs_path,
                               const std::string& repo_root);

/// Normalize path separators to forward slashes and collapse redundant segments.
std::string NormalizePosix(const std::string& path);

/// Return true when *path* names a system header (lives under /usr, /Library,
/// or the compiler's built-in include directories).
bool IsSystemPath(const std::string& path);

} // namespace prism::support
