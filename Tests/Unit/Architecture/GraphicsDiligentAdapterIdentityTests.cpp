/// @file GraphicsDiligentAdapterIdentityTests.cpp
/// @brief Guards split Diligent graphics adapter test identity metadata.

#include <gtest/gtest.h>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

struct TestIdentity
{
    std::string suite;
    std::string name;
    std::filesystem::path path;
    std::size_t line = 0;
};

std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::size_t CountLines(std::string_view text)
{
    std::size_t lines = 1u;
    for (char c : text)
    {
        if (c == '\n')
        {
            ++lines;
        }
    }
    return lines;
}

std::size_t LineForOffset(std::string_view text, std::size_t offset)
{
    std::size_t line = 1u;
    for (std::size_t i = 0; i < offset && i < text.size(); ++i)
    {
        if (text[i] == '\n')
        {
            ++line;
        }
    }
    return line;
}

std::string StripCommentsAndStrings(std::string_view text)
{
    enum class State
    {
        Code,
        LineComment,
        BlockComment,
        StringLiteral,
        CharLiteral,
    };

    std::string stripped(text);
    State state = State::Code;
    bool escaped = false;

    for (std::size_t i = 0; i < stripped.size(); ++i)
    {
        const char c = stripped[i];
        const char next = i + 1u < stripped.size() ? stripped[i + 1u] : '\0';

        switch (state)
        {
        case State::Code:
            if (c == '/' && next == '/')
            {
                stripped[i] = ' ';
                stripped[i + 1u] = ' ';
                ++i;
                state = State::LineComment;
            }
            else if (c == '/' && next == '*')
            {
                stripped[i] = ' ';
                stripped[i + 1u] = ' ';
                ++i;
                state = State::BlockComment;
            }
            else if (c == '"')
            {
                stripped[i] = ' ';
                escaped = false;
                state = State::StringLiteral;
            }
            else if (c == '\'')
            {
                stripped[i] = ' ';
                escaped = false;
                state = State::CharLiteral;
            }
            break;

        case State::LineComment:
            if (c == '\n')
            {
                state = State::Code;
            }
            else
            {
                stripped[i] = ' ';
            }
            break;

        case State::BlockComment:
            if (c == '*' && next == '/')
            {
                stripped[i] = ' ';
                stripped[i + 1u] = ' ';
                ++i;
                state = State::Code;
            }
            else if (c != '\n')
            {
                stripped[i] = ' ';
            }
            break;

        case State::StringLiteral:
        case State::CharLiteral:
            if (c != '\n')
            {
                stripped[i] = ' ';
            }
            if (!escaped &&
                ((state == State::StringLiteral && c == '"') ||
                 (state == State::CharLiteral && c == '\'')))
            {
                state = State::Code;
            }
            escaped = !escaped && c == '\\';
            if (c != '\\')
            {
                escaped = false;
            }
            break;
        }
    }

    return stripped;
}

void SkipWhitespace(std::string_view text, std::size_t& cursor)
{
    while (cursor < text.size() &&
           std::isspace(static_cast<unsigned char>(text[cursor])) != 0)
    {
        ++cursor;
    }
}

std::string ParseIdentifier(std::string_view text, std::size_t& cursor)
{
    SkipWhitespace(text, cursor);
    const auto begin = cursor;
    while (cursor < text.size())
    {
        const char c = text[cursor];
        if (std::isalnum(static_cast<unsigned char>(c)) == 0 && c != '_')
        {
            break;
        }
        ++cursor;
    }
    return std::string(text.substr(begin, cursor - begin));
}

std::optional<TestIdentity> TryParseTestIdentity(
    std::string_view text,
    std::size_t offset,
    const std::filesystem::path& path)
{
    std::size_t cursor = offset;
    if (text.substr(cursor, 4) == "TEST")
    {
        cursor += 4u;
    }
    else
    {
        return std::nullopt;
    }

    if (text.substr(cursor, 2) == "_F" || text.substr(cursor, 2) == "_P")
    {
        cursor += 2u;
    }

    SkipWhitespace(text, cursor);
    if (cursor >= text.size() || text[cursor] != '(')
    {
        return std::nullopt;
    }
    ++cursor;

    auto suite = ParseIdentifier(text, cursor);
    SkipWhitespace(text, cursor);
    if (suite.empty() || cursor >= text.size() || text[cursor] != ',')
    {
        return std::nullopt;
    }
    ++cursor;

    auto name = ParseIdentifier(text, cursor);
    if (name.empty())
    {
        return std::nullopt;
    }

    return TestIdentity{suite, name, path, LineForOffset(text, offset)};
}

std::vector<TestIdentity> ParseTestIdentities(
    const std::filesystem::path& path)
{
    const auto text = ReadText(path);
    const auto searchable = StripCommentsAndStrings(text);
    std::vector<TestIdentity> identities;
    std::size_t cursor = 0;
    while (cursor < searchable.size())
    {
        const auto offset = searchable.find("TEST", cursor);
        if (offset == std::string::npos)
        {
            break;
        }

        if (auto identity = TryParseTestIdentity(searchable, offset, path))
        {
            identities.push_back(*identity);
        }
        cursor = offset + 4u;
    }
    return identities;
}

bool IsDiligentAdapterSplitTestSource(const std::filesystem::path& path)
{
    const auto filename = path.filename().string();
    return path.extension() == ".cpp" &&
           filename.rfind("GraphicsDiligentBackend", 0) == 0 &&
           filename.find("Tests") != std::string::npos &&
           filename != "GraphicsDiligentBackendAdapterTests.cpp";
}

std::filesystem::path DiligentAdapterTestHelperPath()
{
    return std::filesystem::path(SAGA_SOURCE_ROOT) / "Tests" / "Unit" /
           "Render" / "GraphicsDiligentBackendTestHelpers.h";
}

} // namespace

TEST(GraphicsDiligentAdapterIdentityTests, SplitSourcesKeepUniqueTestIdentities)
{
    const auto renderRoot = std::filesystem::path(SAGA_SOURCE_ROOT) / "Tests" /
                            "Unit" / "Render";
    const auto monolith =
        renderRoot / "GraphicsDiligentBackendAdapterTests.cpp";
    EXPECT_FALSE(std::filesystem::exists(monolith))
        << "The old monolithic Diligent adapter test source must not return.";

    std::vector<std::filesystem::path> sources;
    for (const auto& entry : std::filesystem::directory_iterator(renderRoot))
    {
        if (entry.is_regular_file() &&
            IsDiligentAdapterSplitTestSource(entry.path()))
        {
            sources.push_back(entry.path());
        }
    }
    ASSERT_FALSE(sources.empty());

    std::map<std::string, TestIdentity> seen;
    for (const auto& source : sources)
    {
        const auto text = ReadText(source);
        EXPECT_LE(CountLines(text), 650u)
            << source.generic_string()
            << " is growing back toward a monolithic adapter test file.";

        const auto identities = ParseTestIdentities(source);
        EXPECT_FALSE(identities.empty())
            << source.generic_string()
            << " should contain at least one adapter test.";

        for (const auto& identity : identities)
        {
            const auto key = identity.suite + "." + identity.name;
            const auto [it, inserted] = seen.emplace(key, identity);
            EXPECT_TRUE(inserted)
                << "Duplicate Diligent adapter test identity " << key
                << " in " << identity.path.generic_string() << ":"
                << identity.line << " and " << it->second.path.generic_string()
                << ":" << it->second.line;
        }
    }

    EXPECT_GE(seen.size(), 66u)
        << "The split adapter suite should preserve the old adapter coverage "
           "scale without relying on a generated identity snapshot.";
}

TEST(GraphicsDiligentAdapterIdentityTests, TestHelperStaysFocused)
{
    const auto helper = DiligentAdapterTestHelperPath();
    ASSERT_TRUE(std::filesystem::exists(helper));
    EXPECT_LE(CountLines(ReadText(helper)), 450u)
        << helper.generic_string()
        << " is growing into a stateful adapter god-helper.";
}
