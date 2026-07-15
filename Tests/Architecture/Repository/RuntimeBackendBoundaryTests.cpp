/// @file RuntimeBackendBoundaryTests.cpp
/// @brief Guards private ownership of native render and persistence backends.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
bool IsCppFile(const std::filesystem::path& path)
{
    const auto extension = path.extension().string();
    return extension == ".h" || extension == ".hh" ||
           extension == ".hpp" || extension == ".hxx" ||
           extension == ".c" || extension == ".cc" ||
           extension == ".cpp" || extension == ".cxx";
}

bool IsHeader(const std::filesystem::path& path)
{
    const auto extension = path.extension().string();
    return extension == ".h" || extension == ".hh" ||
           extension == ".hpp" || extension == ".hxx";
}

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

bool HasAnyToken(
    std::string_view text,
    std::initializer_list<std::string_view> tokens)
{
    for (const auto token : tokens)
    {
        if (text.find(token) != std::string_view::npos)
        {
            return true;
        }
    }
    return false;
}

std::string Relative(const std::filesystem::path& path)
{
    return std::filesystem::relative(
               path, std::filesystem::path(SAGA_SOURCE_ROOT))
        .generic_string();
}
} // namespace

TEST(RuntimeBackendBoundaryTests, DiligentImplementationStaysInRhiOrRenderPrivate)
{
    const auto runtimeRoot =
        std::filesystem::path(SAGA_SOURCE_ROOT) / "Engine/Source/Runtime";
    std::vector<std::string> offenders;
    std::size_t diligentSourceCount = 0;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(runtimeRoot))
    {
        if (!entry.is_regular_file() || !IsCppFile(entry.path()))
        {
            continue;
        }

        const auto relative = Relative(entry.path());
        const auto code = StripCommentsAndLiterals(ReadText(entry.path()));
        const bool isDiligentSource =
            relative.find("Diligent") != std::string::npos ||
            HasAnyToken(code, {"Diligent::", "RefCntAutoPtr<"});
        if (!isDiligentSource)
        {
            continue;
        }

        ++diligentSourceCount;
        const bool ownedByRhiPrivate =
            relative.rfind("Engine/Source/Runtime/RHI/Private/", 0) == 0;
        const bool ownedByRenderPrivate =
            relative.rfind("Engine/Source/Runtime/Render/Private/", 0) == 0;
        if (!ownedByRhiPrivate && !ownedByRenderPrivate)
        {
            offenders.push_back(relative);
        }
    }

    EXPECT_GT(diligentSourceCount, 0u);
    EXPECT_TRUE(offenders.empty())
        << "Diligent implementation escaped RHI/Render Private ownership: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(RuntimeBackendBoundaryTests, DiligentDeviceFactoryIsTheOnlyNativeLifecycleCreator)
{
    const auto runtimeRoot =
        std::filesystem::path(SAGA_SOURCE_ROOT) / "Engine/Source/Runtime";
    const std::string expectedOwner =
        "Engine/Source/Runtime/RHI/Private/SagaEngine/Graphics/Backends/"
        "Diligent/Runtime/DiligentDeviceFactory.cpp";
    std::vector<std::string> owners;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(runtimeRoot))
    {
        if (!entry.is_regular_file() ||
            entry.path().extension() != ".cpp")
        {
            continue;
        }
        const auto code = StripCommentsAndLiterals(ReadText(entry.path()));
        if (HasAnyToken(
                code,
                {"CreateDeviceAndContexts", "CreateSwapChain",
                 "CreateDeviceAndSwapChain"}))
        {
            owners.push_back(Relative(entry.path()));
        }
    }

    ASSERT_EQ(owners.size(), 1u)
        << "Native Diligent lifecycle creation must have one source owner";
    EXPECT_EQ(owners.front(), expectedOwner);
}

TEST(RuntimeBackendBoundaryTests, WaitForIdleIsLimitedToDiagnosticFrameCapture)
{
    const auto runtimeRoot =
        std::filesystem::path(SAGA_SOURCE_ROOT) / "Engine/Source/Runtime";
    const std::string expectedDiagnosticOwner =
        "Engine/Source/Runtime/Render/Private/SagaEngine/Render/Backend/"
        "Diligent/DiligentFrameCapture.cpp";
    std::vector<std::string> owners;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(runtimeRoot))
    {
        if (!entry.is_regular_file() || !IsCppFile(entry.path()))
        {
            continue;
        }
        const auto code = StripCommentsAndLiterals(ReadText(entry.path()));
        if (code.find("WaitForIdle") != std::string::npos)
        {
            owners.push_back(Relative(entry.path()));
        }
    }

    ASSERT_EQ(owners.size(), 1u)
        << "Normal BeginFrame/Submit/EndFrame paths must not globally idle";
    EXPECT_EQ(owners.front(), expectedDiagnosticOwner);
}

TEST(RuntimeBackendBoundaryTests, PersistenceVendorDetailsStayPrivate)
{
    const auto persistenceRoot =
        std::filesystem::path(SAGA_SOURCE_ROOT) /
        "Engine/Source/Runtime/Persistence";
    const auto publicRoot = persistenceRoot / "Public";
    std::vector<std::string> publicOffenders;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(publicRoot))
    {
        if (!entry.is_regular_file() || !IsHeader(entry.path()))
        {
            continue;
        }
        const auto code = StripCommentsAndLiterals(ReadText(entry.path()));
        if (HasAnyToken(
                code,
                {"pqxx::", "sw::redis::", "redisContext", "redisReply",
                 "redisAsyncContext"}))
        {
            publicOffenders.push_back(Relative(entry.path()));
        }
    }

    EXPECT_TRUE(publicOffenders.empty())
        << "Persistence public headers expose database vendor types: "
        << (publicOffenders.empty() ? "" : publicOffenders.front());

    const auto postgresqlSource =
        persistenceRoot /
        "Private/SagaEngine/Persistence/Backends/PostgreSQL/PostgreSQLImpl.cpp";
    const auto redisSource =
        persistenceRoot /
        "Private/SagaEngine/Persistence/Backends/Redis/RedisImpl.cpp";
    const auto postgresqlHeader =
        persistenceRoot /
        "Private/SagaEngine/Persistence/Backends/PostgreSQL/PostgreSQLImpl.h";
    const auto redisHeader =
        persistenceRoot /
        "Private/SagaEngine/Persistence/Backends/Redis/RedisImpl.h";
    ASSERT_TRUE(std::filesystem::is_regular_file(postgresqlSource));
    ASSERT_TRUE(std::filesystem::is_regular_file(redisSource));
    ASSERT_TRUE(std::filesystem::is_regular_file(postgresqlHeader));
    ASSERT_TRUE(std::filesystem::is_regular_file(redisHeader));
    EXPECT_FALSE(std::filesystem::exists(
        publicRoot / "SagaEngine/Persistence/Database/PostgreSQLImpl.h"));
    EXPECT_FALSE(std::filesystem::exists(
        publicRoot / "SagaEngine/Persistence/Database/RedisImpl.h"));
    EXPECT_TRUE(HasAnyToken(
        StripCommentsAndLiterals(ReadText(postgresqlSource)), {"pqxx::"}));
    EXPECT_TRUE(HasAnyToken(
        StripCommentsAndLiterals(ReadText(redisSource)), {"sw::redis::"}));

    std::vector<std::string> vendorSourceOffenders;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(persistenceRoot))
    {
        if (!entry.is_regular_file() ||
            entry.path().extension() != ".cpp")
        {
            continue;
        }
        const auto code = StripCommentsAndLiterals(ReadText(entry.path()));
        if (HasAnyToken(code, {"pqxx::", "sw::redis::"}) &&
            Relative(entry.path()).find("/Private/") == std::string::npos)
        {
            vendorSourceOffenders.push_back(Relative(entry.path()));
        }
    }
    EXPECT_TRUE(vendorSourceOffenders.empty())
        << "Persistence vendor source escaped Private ownership: "
        << (vendorSourceOffenders.empty() ? "" : vendorSourceOffenders.front());
}

TEST(RuntimeBackendBoundaryTests, RmlUiAdapterStaysPrivate)
{
    const auto uiRoot =
        std::filesystem::path(SAGA_SOURCE_ROOT) / "Engine/Source/Runtime/UI";
    const auto privateAdapter = uiRoot / "Private/Backends/RmlUi";

    EXPECT_FALSE(std::filesystem::exists(
        uiRoot / "Public/SagaEngine/UI/Backends/RmlUiUiBackend.h"));
    EXPECT_TRUE(std::filesystem::is_regular_file(
        privateAdapter / "RmlUiUiBackend.h"));
    EXPECT_TRUE(std::filesystem::is_regular_file(
        privateAdapter / "RmlUiUiBackend.cpp"));
}
