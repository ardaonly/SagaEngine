#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string StripCommentsAndLiterals(std::string_view text)
{
    enum class State
    {
        Code,
        LineComment,
        BlockComment,
        StringLiteral,
        CharLiteral,
    };

    std::string result(text);
    State state = State::Code;
    bool escaped = false;
    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const char c = result[i];
        const char next = i + 1 < result.size() ? result[i + 1] : '\0';
        switch (state)
        {
        case State::Code:
            if (c == '/' && next == '/')
            {
                result[i] = result[i + 1] = ' ';
                ++i;
                state = State::LineComment;
            }
            else if (c == '/' && next == '*')
            {
                result[i] = result[i + 1] = ' ';
                ++i;
                state = State::BlockComment;
            }
            else if (c == '"')
            {
                result[i] = ' ';
                escaped = false;
                state = State::StringLiteral;
            }
            else if (c == '\'')
            {
                result[i] = ' ';
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
                result[i] = ' ';
            }
            break;
        case State::BlockComment:
            if (c == '*' && next == '/')
            {
                result[i] = result[i + 1] = ' ';
                ++i;
                state = State::Code;
            }
            else if (c != '\n')
            {
                result[i] = ' ';
            }
            break;
        case State::StringLiteral:
        case State::CharLiteral:
            if (c != '\n')
            {
                result[i] = ' ';
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
    return result;
}

std::string Relative(const std::filesystem::path& path)
{
    return std::filesystem::relative(
               path, std::filesystem::path(SAGA_SOURCE_ROOT))
        .generic_string();
}
} // namespace

TEST(EngineServerBoundaryTests, CanonicalServerBoundaryRootsExist)
{
    const auto runtimeRoot =
        std::filesystem::path(SAGA_SOURCE_ROOT) / "Engine/Source/Runtime";
    for (const auto* module : {"Networking", "Replication", "ServerAuthority"})
    {
        EXPECT_TRUE(std::filesystem::is_directory(
            runtimeRoot / module / "Public"))
            << module;
        EXPECT_TRUE(std::filesystem::is_directory(
            runtimeRoot / module / "Private"))
            << module;
    }
}

TEST(EngineServerBoundaryTests, NetworkingDoesNotOwnAuthorityTypes)
{
    const auto networkingRoot =
        std::filesystem::path(SAGA_SOURCE_ROOT) /
        "Engine/Source/Runtime/Networking";
    const std::array authorityTypes = {
        "ActorOwnershipRegistry",
        "AuthoritativeMovementCommandIntake",
        "AuthoritativeMovementCore",
        "AuthoritativeMovementInputAdapter",
        "GameplayCommandDispatcher",
        "InputCommandQueue",
        "MovementValidator",
        "ShardManager",
        "ZoneServer",
    };
    std::vector<std::filesystem::path> offenders;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(networkingRoot))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        const auto code = StripCommentsAndLiterals(ReadText(entry.path()));
        bool ownsAuthorityType = false;
        for (const auto* type : authorityTypes)
        {
            const std::regex declaration(
                std::string("\\b(class|struct)\\s+") + type + "\\b");
            if (entry.path().stem().string().find(type) != std::string::npos ||
                std::regex_search(code, declaration))
            {
                ownsAuthorityType = true;
                break;
            }
        }
        if (ownsAuthorityType)
        {
            offenders.push_back(entry.path());
        }
    }
    EXPECT_TRUE(offenders.empty())
        << "Authority implementation leaked into Networking: "
        << (offenders.empty() ? "" : Relative(offenders.front()));
}

TEST(EngineServerBoundaryTests, ConnectionImplementationsStayServerAuthorityPrivate)
{
    const auto runtimeRoot =
        std::filesystem::path(SAGA_SOURCE_ROOT) / "Engine/Source/Runtime";
    const auto ownerRoot =
        runtimeRoot / "ServerAuthority/Private/SagaEngine/ServerAuthority";
    const std::array expectedFiles = {
        ownerRoot / "ConnectionManager.cpp",
        ownerRoot / "ConnectionManager.h",
        ownerRoot / "ServerConnection.cpp",
        ownerRoot / "ServerConnection.h",
    };
    for (const auto& expected : expectedFiles)
    {
        ASSERT_TRUE(std::filesystem::is_regular_file(expected)) << expected;
    }

    std::vector<std::filesystem::path> owners;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(runtimeRoot))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }
        const auto stem = entry.path().stem().string();
        if (stem == "ConnectionManager" || stem == "ServerConnection")
        {
            owners.push_back(entry.path());
        }
    }

    ASSERT_EQ(owners.size(), expectedFiles.size());
    for (const auto& owner : owners)
    {
        EXPECT_EQ(owner.parent_path(), ownerRoot)
            << "Connection implementation must remain ServerAuthority Private: "
            << Relative(owner);
    }
}
