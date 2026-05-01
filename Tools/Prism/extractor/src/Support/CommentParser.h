/// @file Support/CommentParser.h
/// @brief Brief-comment extraction from raw Doxygen / triple-slash strings.

#pragma once

#include <string>

namespace prism::support {

/// Extract the first non-empty documentation sentence from *raw_comment*.
///
/// Recognition rules (evaluated in order per line):
///   1. Lines starting with @brief → return the text after the tag.
///   2. Lines starting with any other @ tag → skip (e.g. @file, @author).
///   3. First remaining non-empty line → returned as-is.
///
/// Both /// and block-comment styles ( /** … */ ) are handled.
/// Returns an empty string when *raw_comment* is empty or tag-only.
std::string ExtractBrief(const std::string& raw_comment);

} // namespace prism::support
