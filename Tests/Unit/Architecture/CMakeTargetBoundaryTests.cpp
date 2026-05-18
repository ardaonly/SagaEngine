/// @file CMakeTargetBoundaryTests.cpp
/// @brief Guards critical CMake target dependency boundaries.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

struct CMakeLinkCall
{
    std::string target; ///< First argument passed to target_link_libraries.
    std::string text;   ///< Full call text collected from SagaTargets.cmake.
    std::size_t line = 0; ///< 1-based source line for failure diagnostics.
};

std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::vector<std::string> ReadLines(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line))
    {
        lines.push_back(line);
    }
    return lines;
}

std::string Trim(std::string_view value)
{
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos)
    {
        return {};
    }

    const auto end = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(begin, end - begin + 1));
}

bool StartsWith(std::string_view value, std::string_view prefix)
{
    return value.size() >= prefix.size() &&
           value.substr(0, prefix.size()) == prefix;
}

std::string ExtractFirstArgument(const std::string& line)
{
    constexpr std::string_view marker = "target_link_libraries(";
    const auto start = line.find(marker);
    if (start == std::string::npos)
    {
        return {};
    }

    auto cursor = start + marker.size();
    while (cursor < line.size() &&
           (line[cursor] == ' ' || line[cursor] == '\t'))
    {
        ++cursor;
    }

    const auto end = line.find_first_of(" \t\r\n)", cursor);
    if (end == std::string::npos)
    {
        return line.substr(cursor);
    }

    return line.substr(cursor, end - cursor);
}

std::vector<CMakeLinkCall> ExtractTargetLinkCalls(
    const std::vector<std::string>& lines)
{
    std::vector<CMakeLinkCall> calls;

    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const auto& line = lines[i];
        if (line.find("target_link_libraries(") == std::string::npos)
        {
            continue;
        }

        CMakeLinkCall call;
        call.target = ExtractFirstArgument(line);
        call.line = i + 1;
        call.text = line + "\n";

        while (call.text.find(')') == std::string::npos &&
               i + 1 < lines.size())
        {
            ++i;
            call.text += lines[i] + "\n";
        }

        calls.push_back(std::move(call));
    }

    return calls;
}

bool ContainsToken(const std::string& text, const std::string& token)
{
    return text.find(token) != std::string::npos;
}

std::vector<CMakeLinkCall> FindForbiddenLinks(
    const std::vector<CMakeLinkCall>& calls,
    const std::string& target,
    const std::vector<std::string>& forbiddenDependencies)
{
    std::vector<CMakeLinkCall> offenders;
    for (const auto& call : calls)
    {
        if (call.target != target)
        {
            continue;
        }

        for (const auto& dependency : forbiddenDependencies)
        {
            if (ContainsToken(call.text, dependency))
            {
                offenders.push_back(call);
                break;
            }
        }
    }

    return offenders;
}

std::vector<std::string> FindForbiddenDependencyNames(
    const CMakeLinkCall& call,
    const std::vector<std::string>& forbiddenDependencies)
{
    std::vector<std::string> names;
    for (const auto& dependency : forbiddenDependencies)
    {
        if (ContainsToken(call.text, dependency))
        {
            names.push_back(dependency);
        }
    }
    return names;
}

std::string JoinNames(const std::vector<std::string>& names)
{
    std::string result;
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        if (i != 0)
        {
            result += ", ";
        }
        result += names[i];
    }
    return result;
}

bool IsLineInsideIfGuard(
    const std::vector<std::string>& lines,
    std::size_t lineIndex,
    const std::string& guardName)
{
    std::vector<std::string> guardStack;
    for (std::size_t i = 0; i <= lineIndex && i < lines.size(); ++i)
    {
        const std::string trimmed = Trim(lines[i]);
        if (StartsWith(trimmed, "if("))
        {
            guardStack.push_back(trimmed);
            continue;
        }

        if (StartsWith(trimmed, "endif"))
        {
            if (!guardStack.empty())
            {
                guardStack.pop_back();
            }
        }
    }

    for (const auto& guard : guardStack)
    {
        if (guard.find(guardName) != std::string::npos)
        {
            return true;
        }
    }

    return false;
}

std::filesystem::path SagaTargetsPath()
{
    return std::filesystem::path(SAGA_SOURCE_ROOT) /
           "cmake" / "modules" / "SagaTargets.cmake";
}

} // namespace

TEST(CMakeTargetBoundaryTests, SagaProductLibDoesNotLinkEditorLabTargets)
{
    const auto path = SagaTargetsPath();
    const auto calls = ExtractTargetLinkCalls(ReadLines(path));
    const std::vector<std::string> forbidden = {
        "SagaEditorLabLib",
        "SagaEditorLabBridge",
    };

    const auto offenders =
        FindForbiddenLinks(calls, "SagaProductLib", forbidden);

    for (const auto& offender : offenders)
    {
        ADD_FAILURE()
            << "Forbidden target dependency: SagaProductLib must not link "
            << JoinNames(FindForbiddenDependencyNames(offender, forbidden))
            << ". Offending call in " << path.generic_string()
            << ":" << offender.line << "\n" << offender.text;
    }
}

TEST(CMakeTargetBoundaryTests, SagaAssetPipelineLibDoesNotLinkRuntimeEditorProductOrToolOwners)
{
    const auto path = SagaTargetsPath();
    const auto calls = ExtractTargetLinkCalls(ReadLines(path));
    const std::vector<std::string> forbidden = {
        "SagaRuntimeLib",
        "SagaEditorLib",
        "SagaProductLib",
        "Forge",
        "Prism",
        "SDE",
    };

    const auto offenders =
        FindForbiddenLinks(calls, "SagaAssetPipelineLib", forbidden);

    for (const auto& offender : offenders)
    {
        ADD_FAILURE()
            << "Forbidden target dependency: SagaAssetPipelineLib must not link "
            << JoinNames(FindForbiddenDependencyNames(offender, forbidden))
            << ". Offending call in " << path.generic_string()
            << ":" << offender.line << "\n" << offender.text;
    }
}

TEST(CMakeTargetBoundaryTests, RuntimeAndServerTargetsDoNotLinkEditorDevOrToolTargets)
{
    const auto path = SagaTargetsPath();
    const auto calls = ExtractTargetLinkCalls(ReadLines(path));
    const std::vector<std::string> runtimeTargets = {
        "SagaRuntimeLib",
        "SagaServerLib",
        "SagaRuntime",
        "SagaServer",
    };
    const std::vector<std::string> forbidden = {
        "SagaEditorLib",
        "SagaEditorLabLib",
        "SagaEditorLabBridge",
        "SagaProductLib",
        "SagaAssetPipelineLib",
        "Forge",
        "Prism",
        "SDE",
    };

    for (const auto& target : runtimeTargets)
    {
        const auto offenders = FindForbiddenLinks(calls, target, forbidden);
        for (const auto& offender : offenders)
        {
            ADD_FAILURE()
                << "Forbidden target dependency: " << target
                << " must not link "
                << JoinNames(FindForbiddenDependencyNames(offender, forbidden))
                << ". Offending call in " << path.generic_string()
                << ":" << offender.line << "\n" << offender.text;
        }
    }
}

TEST(CMakeTargetBoundaryTests, EditorLabBridgeIsGuardedByDevPanelFlag)
{
    const auto path = SagaTargetsPath();
    const auto lines = ReadLines(path);
    constexpr const char* guardName = "SAGA_WITH_EDITORLAB_DEV_PANEL";

    bool sawBridgeTarget = false;
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const bool mentionsBridge =
            lines[i].find("SagaEditorLabBridge") != std::string::npos;
        if (!mentionsBridge)
        {
            continue;
        }

        sawBridgeTarget = true;
        EXPECT_TRUE(IsLineInsideIfGuard(lines, i, guardName))
            << "SagaEditorLabBridge must only appear inside if("
            << guardName << "). Offending line in " << path.generic_string()
            << ":" << (i + 1) << ": " << lines[i];
    }

    EXPECT_TRUE(sawBridgeTarget)
        << "Expected SagaEditorLabBridge dev-only bridge target in "
        << path.generic_string();
}

TEST(CMakeTargetBoundaryTests, SagaLinksEditorLabBridgeOnlyBehindDevPanelFlag)
{
    const auto path = SagaTargetsPath();
    const auto lines = ReadLines(path);
    const auto calls = ExtractTargetLinkCalls(lines);
    constexpr const char* guardName = "SAGA_WITH_EDITORLAB_DEV_PANEL";

    bool sawSagaBridgeLink = false;
    for (const auto& call : calls)
    {
        if (call.target != "Saga" ||
            !ContainsToken(call.text, "SagaEditorLabBridge"))
        {
            continue;
        }

        sawSagaBridgeLink = true;
        ASSERT_GT(call.line, 0u);
        EXPECT_TRUE(IsLineInsideIfGuard(lines, call.line - 1, guardName))
            << "Forbidden unguarded dependency: Saga may link "
            << "SagaEditorLabBridge only inside if(" << guardName
            << "). Offending call in " << path.generic_string()
            << ":" << call.line << "\n" << call.text;
    }

    EXPECT_TRUE(sawSagaBridgeLink)
        << "Expected Saga executable to link SagaEditorLabBridge behind if("
        << guardName << ") in " << path.generic_string();
}
