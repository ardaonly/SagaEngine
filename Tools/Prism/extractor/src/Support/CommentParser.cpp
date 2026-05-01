/// @file Support/CommentParser.cpp
/// @brief Implementation of brief-comment extraction.

#include "CommentParser.h"

#include <sstream>

namespace prism::support {

// ─── Internal ─────────────────────────────────────────────────────────────────

static std::string TrimWS(const std::string& s)
{
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

/// Strip one comment-marker prefix from *raw_line* and return the inner text.
/// Handles both `///` (triple-slash) and ` * ` (block-comment) styles.
static std::string StripMarker(const std::string& raw_line)
{
    std::string line = raw_line;

    // Consume leading horizontal whitespace.
    const size_t nonws = line.find_first_not_of(" \t");
    if (nonws == std::string::npos) return {};
    line = line.substr(nonws);

    // Triple-slash style: `///`
    if (line.size() >= 3 && line[0] == '/' && line[1] == '/' && line[2] == '/')
    {
        line = line.substr(3);
        if (!line.empty() && line[0] == ' ') line = line.substr(1);
        return line;
    }

    // Block-comment style: ` * text` or `** text`
    if (!line.empty() && line[0] == '*')
    {
        line = line.substr(1);
        if (!line.empty() && line[0] == ' ') line = line.substr(1);
        return line;
    }

    return line;
}

// ─── Public API ───────────────────────────────────────────────────────────────

std::string ExtractBrief(const std::string& raw_comment)
{
    if (raw_comment.empty())
        return {};

    std::istringstream stream(raw_comment);
    std::string        line;

    while (std::getline(stream, line))
    {
        const std::string inner = TrimWS(StripMarker(line));

        if (inner.empty())
            continue;

        // Explicit @brief tag: return the rest of the line.
        if (inner.rfind("@brief", 0) == 0)
            return TrimWS(inner.substr(6));

        // Any other @tag line: skip over it.
        if (inner[0] == '@')
            continue;

        return inner;
    }

    return {};
}

} // namespace prism::support
