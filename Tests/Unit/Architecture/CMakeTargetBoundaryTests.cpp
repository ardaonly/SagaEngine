/// @file CMakeTargetBoundaryTests.cpp
/// @brief Guards critical CMake target dependency boundaries.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <map>
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

std::string ExtractTargetCall(
    const std::string& text,
    std::string_view command,
    std::string_view target)
{
    const std::string marker =
        std::string(command) + "(" + std::string(target);
    const auto start = text.find(marker);
    if (start == std::string::npos)
    {
        return {};
    }

    const auto end = text.find(')', start);
    if (end == std::string::npos)
    {
        return text.substr(start);
    }
    return text.substr(start, end - start + 1);
}

bool ContainsToken(const std::string& text, const std::string& token)
{
    return text.find(token) != std::string::npos;
}

bool IsCodeOrBuildFile(const std::filesystem::path& path)
{
    const auto filename = path.filename().string();
    const auto extension = path.extension().string();
    return filename == "CMakeLists.txt" ||
           extension == ".h" ||
           extension == ".hpp" ||
           extension == ".cpp" ||
           extension == ".rs" ||
           extension == ".py";
}

std::vector<std::string> FindForbiddenText(
    const std::filesystem::path& root,
    const std::vector<std::string>& forbiddenTokens)
{
    std::vector<std::string> offenders;
    if (!std::filesystem::exists(root))
    {
        return offenders;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file() || !IsCodeOrBuildFile(entry.path()))
        {
            continue;
        }

        const std::string text = ReadText(entry.path());
        for (const std::string& token : forbiddenTokens)
        {
            if (text.find(token) != std::string::npos)
            {
                offenders.push_back(
                    entry.path().generic_string() + ": " + token);
                break;
            }
        }
    }
    return offenders;
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

std::filesystem::path CMakeListsPath()
{
    return std::filesystem::path(SAGA_SOURCE_ROOT) / "CMakeLists.txt";
}

std::filesystem::path CMakeModulePath(std::string_view filename)
{
    return std::filesystem::path(SAGA_SOURCE_ROOT) /
           "cmake" / "modules" / std::string(filename);
}

std::filesystem::path BuildNinjaPath()
{
    return std::filesystem::path(SAGA_BUILD_ROOT) / "build.ninja";
}

std::size_t CountLines(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::size_t count = 0;
    std::string line;
    while (std::getline(input, line))
    {
        ++count;
    }
    return count;
}

std::vector<std::filesystem::path> FindFilesWithExtension(
    const std::filesystem::path& root,
    std::string_view extension)
{
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::exists(root))
    {
        return files;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
    {
        if (entry.is_regular_file() &&
            entry.path().extension().string() == extension)
        {
            files.push_back(entry.path());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

std::string RelativeToSourceRoot(const std::filesystem::path& path)
{
    return std::filesystem::relative(
        path,
        std::filesystem::path(SAGA_SOURCE_ROOT)).generic_string();
}

std::string ExtractGTestIdentity(const std::string& line)
{
    constexpr std::array<std::string_view, 2> markers = {
        "TEST_F(",
        "TEST(",
    };

    for (const auto marker : markers)
    {
        const auto start = line.find(marker);
        if (start == std::string::npos)
        {
            continue;
        }

        const auto args = start + marker.size();
        const auto comma = line.find(',', args);
        const auto close = line.find(')', comma == std::string::npos ? args : comma);
        if (comma == std::string::npos || close == std::string::npos)
        {
            return {};
        }

        return Trim(std::string_view(line).substr(args, comma - args)) +
               "." +
               Trim(std::string_view(line).substr(comma + 1, close - comma - 1));
    }

    return {};
}

std::map<std::string, int> ExtractGTestIdentityCounts(
    const std::filesystem::path& root)
{
    std::map<std::string, int> counts;
    for (const auto& file : FindFilesWithExtension(root, ".cpp"))
    {
        for (const auto& line : ReadLines(file))
        {
            const auto identity = ExtractGTestIdentity(line);
            if (!identity.empty())
            {
                ++counts[identity];
            }
        }
    }
    return counts;
}

std::size_t CountOccurrences(std::string_view text, std::string_view needle)
{
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string_view::npos)
    {
        ++count;
        pos += needle.size();
    }
    return count;
}

} // namespace

TEST(CMakeTargetBoundaryTests, DiligentGpuIntegrationSplitDoesNotRegressToMonolith)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto renderRoot = root / "Tests" / "Integration" / "Render";
    const auto diligentRoot = renderRoot / "Diligent";

    EXPECT_FALSE(std::filesystem::exists(
        renderRoot / "DiligentBackendIntegrationTests.cpp"))
        << "Diligent GPU integration tests must stay split under "
           "Tests/Integration/Render/Diligent/.";
    ASSERT_TRUE(std::filesystem::exists(diligentRoot));

    for (const auto& file : FindFilesWithExtension(renderRoot, ".cpp"))
    {
        EXPECT_LT(CountLines(file), 2000u)
            << "Integration render test source is too large: "
            << RelativeToSourceRoot(file);
    }

    for (const auto& file : FindFilesWithExtension(diligentRoot, ".cpp"))
    {
        EXPECT_LE(CountLines(file), 650u)
            << "Diligent GPU split source exceeded its measured line budget: "
            << RelativeToSourceRoot(file);
    }
}

TEST(CMakeTargetBoundaryTests, DiligentGpuFixtureHeadersStayFocused)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto diligentRoot =
        root / "Tests" / "Integration" / "Render" / "Diligent";
    const std::map<std::string, std::size_t> budgets = {
        {"DiligentGpuTestFixture.h", 130u},
        {"SagaGraphicsGpuTestFixture.h", 150u},
        {"BindingGpuTestFixture.h", 80u},
    };

    for (const auto& [name, budget] : budgets)
    {
        const auto file = diligentRoot / name;
        ASSERT_TRUE(std::filesystem::exists(file))
            << "Missing Diligent GPU fixture header: " << file.generic_string();
        EXPECT_LE(CountLines(file), budget)
            << "Fixture header exceeded its measured line budget: "
            << RelativeToSourceRoot(file);
    }
}

TEST(CMakeTargetBoundaryTests, DiligentGpuIntegrationTestIdentitiesStayStable)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto diligentRoot =
        root / "Tests" / "Integration" / "Render" / "Diligent";
    const auto identities = ExtractGTestIdentityCounts(diligentRoot);

    std::map<std::string, int> suiteCounts;
    for (const auto& [identity, count] : identities)
    {
        EXPECT_EQ(count, 1) << "Duplicate GPU integration test identity: "
                            << identity;
        const auto dot = identity.find('.');
        ASSERT_NE(dot, std::string::npos);
        ++suiteCounts[identity.substr(0, dot)];
    }

    EXPECT_EQ(identities.size(), 137u);
    EXPECT_EQ(suiteCounts["DiligentGPU"], 44);
    EXPECT_EQ(suiteCounts["CoordinateGPU"], 6);
    EXPECT_EQ(suiteCounts["SagaGraphicsGPU"], 18);
    EXPECT_EQ(suiteCounts["BindingGPU"], 52);
    EXPECT_EQ(suiteCounts["OverlayGPU"], 17);
    EXPECT_EQ(suiteCounts.size(), 5u);
}

TEST(CMakeTargetBoundaryTests, DiligentGpuSplitSourcesAreOwnedByIntegrationTargetOnce)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto diligentRoot =
        root / "Tests" / "Integration" / "Render" / "Diligent";
    const auto ninjaPath = BuildNinjaPath();
    ASSERT_TRUE(std::filesystem::exists(ninjaPath))
        << "Generated build.ninja is required for compile ownership checks: "
        << ninjaPath.generic_string();

    const auto ninja = ReadText(ninjaPath);
    EXPECT_EQ(ninja.find("DiligentBackendIntegrationTests.cpp"),
              std::string::npos)
        << "Removed Diligent GPU monolith must not be compiled.";

    for (const auto& file : FindFilesWithExtension(diligentRoot, ".cpp"))
    {
        const auto rel = RelativeToSourceRoot(file);
        const auto object =
            "build CMakeFiles/SagaIntegrationTests.dir/" + rel + ".o:";
        EXPECT_EQ(CountOccurrences(ninja, object), 1u)
            << "Diligent split source must be compiled exactly once by "
               "SagaIntegrationTests: "
            << rel;
    }
}

TEST(CMakeTargetBoundaryTests, DiligentFrameSlotTrackerStaysPrivateAndSingleOwned)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto trackerSource =
        root / "Engine" / "Private" / "SagaEngine" / "Render" /
        "Backend" / "Diligent" / "DiligentFrameSlotTracker.cpp";
    const auto trackerHeader =
        root / "Engine" / "Private" / "SagaEngine" / "Render" /
        "Backend" / "Diligent" / "DiligentFrameSlotTracker.h";
    const auto backendPrivate =
        root / "Engine" / "Private" / "SagaEngine" / "Render" /
        "Backend" / "Diligent" / "DiligentRenderBackendPrivate.h";
    const auto runtimeSource =
        root / "Engine" / "Private" / "SagaEngine" / "Graphics" /
        "Backends" / "Diligent" / "Runtime" / "DiligentGraphicsRuntime.cpp";

    ASSERT_TRUE(std::filesystem::exists(trackerSource));
    ASSERT_TRUE(std::filesystem::exists(trackerHeader));
    ASSERT_TRUE(std::filesystem::exists(backendPrivate));
    ASSERT_TRUE(std::filesystem::exists(runtimeSource));

    const auto trackerHeaderText = ReadText(trackerHeader);
    EXPECT_TRUE(ContainsToken(
        trackerHeaderText,
        "DiligentFrameSlotTracker(const DiligentFrameSlotTracker&) = delete"))
        << "Frame-slot tracker must remain non-copyable.";
    EXPECT_TRUE(ContainsToken(
        trackerHeaderText,
        "DiligentFrameSlotTracker(DiligentFrameSlotTracker&&) = delete"))
        << "Frame-slot tracker must remain non-movable.";

    const auto backendPrivateText = ReadText(backendPrivate);
    EXPECT_EQ(backendPrivateText.find("frameSlotSerials"), std::string::npos)
        << "Raw frame slot serial storage must not return to backend Impl.";
    EXPECT_EQ(backendPrivateText.find("activeFrameSlot"), std::string::npos)
        << "Raw active frame slot storage must not return to backend Impl.";
    EXPECT_EQ(backendPrivateText.find("activeFrameSerial"), std::string::npos)
        << "Raw active frame serial storage must not return to backend Impl.";
    EXPECT_EQ(CountOccurrences(backendPrivateText, "DiligentGpuTimeline gpuTimeline"), 0u)
        << "DiligentRenderBackend must not own the GPU timeline after the "
           "canonical runtime migration.";
    const auto runtimeText = ReadText(runtimeSource);
    EXPECT_EQ(CountOccurrences(runtimeText, "DiligentGpuTimeline gpuTimeline"), 1u)
        << "DiligentGraphicsRuntime must keep exactly one GPU timeline owner.";

    const auto ninjaPath = BuildNinjaPath();
    ASSERT_TRUE(std::filesystem::exists(ninjaPath))
        << "Generated build.ninja is required for compile ownership checks: "
        << ninjaPath.generic_string();
    const auto ninja = ReadText(ninjaPath);
    const auto trackerRel = RelativeToSourceRoot(trackerSource);

    const auto diligentObject =
        "build CMakeFiles/SagaDiligentRuntime.dir/" + trackerRel + ".o:";
    EXPECT_EQ(CountOccurrences(ninja, diligentObject), 1u)
        << "DiligentFrameSlotTracker.cpp must be compiled exactly once by "
           "SagaDiligentRuntime.";

    const auto engineObject =
        "build CMakeFiles/SagaEngine.dir/" + trackerRel + ".o:";
    const auto graphicsPrivateObject =
        "build CMakeFiles/SagaGraphicsPrivate.dir/" + trackerRel + ".o:";
    EXPECT_EQ(CountOccurrences(ninja, engineObject), 0u)
        << "DiligentFrameSlotTracker.cpp must not be compiled by SagaEngine.";
    EXPECT_EQ(CountOccurrences(ninja, graphicsPrivateObject), 0u)
        << "DiligentFrameSlotTracker.cpp must not be compiled by SagaGraphicsPrivate.";

    EXPECT_EQ(ReadText(SagaTargetsPath()).find("DiligentFrameSlotTracker"),
              std::string::npos)
        << "Tracker ownership should come from the private Diligent backend glob.";
    EXPECT_EQ(ReadText(CMakeModulePath("SagaInstall.cmake")).find(
                  "DiligentFrameSlotTracker"),
              std::string::npos)
        << "Tracker header must not be installed.";
    EXPECT_EQ(ReadText(CMakeModulePath("SagaInstallGraphics.cmake")).find(
                  "DiligentFrameSlotTracker"),
              std::string::npos)
        << "Tracker header must not be installed through graphics exports.";
}

TEST(CMakeTargetBoundaryTests, DiligentRuntimeOwnsFrameArenaImplementation)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto runtimeHeader =
        root / "Engine" / "Private" / "SagaEngine" / "Graphics" / "Backends" / "Diligent" / "Runtime" /
        "DiligentGraphicsRuntime.h";
    const auto runtimeSource =
        root / "Engine" / "Private" / "SagaEngine" / "Graphics" / "Backends" / "Diligent" / "Runtime" /
        "DiligentGraphicsRuntime.cpp";
    ASSERT_TRUE(std::filesystem::exists(runtimeHeader));
    ASSERT_TRUE(std::filesystem::exists(runtimeSource));

    const auto headerText = ReadText(runtimeHeader);
    EXPECT_TRUE(ContainsToken(headerText, "FrameToken"));
    EXPECT_TRUE(ContainsToken(headerText, "ConstantSlice"));
    EXPECT_TRUE(ContainsToken(headerText, "DiligentFrameUploadArena"));
    EXPECT_TRUE(ContainsToken(headerText, "GraphicsCommandEncoder"));
    EXPECT_TRUE(ContainsToken(headerText, "GraphicsRenderPassEncoder"));

    const auto ninjaPath = BuildNinjaPath();
    ASSERT_TRUE(std::filesystem::exists(ninjaPath));
    const auto ninja = ReadText(ninjaPath);
    const auto runtimeRel = RelativeToSourceRoot(runtimeSource);
    const auto runtimeObject =
        "build CMakeFiles/SagaDiligentRuntime.dir/" + runtimeRel + ".o:";
    EXPECT_EQ(CountOccurrences(ninja, runtimeObject), 1u)
        << "DiligentGraphicsRuntime.cpp must be compiled by SagaDiligentRuntime.";
}

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

TEST(CMakeTargetBoundaryTests, SagaEngineDoesNotLinkProductEditorOrToolTargets)
{
    const auto path = SagaTargetsPath();
    const auto calls = ExtractTargetLinkCalls(ReadLines(path));
    const std::vector<std::string> forbidden = {
        "SagaProductLib",
        "SagaEditorLib",
        "SagaEditorLabLib",
        "SagaEditorLabBridge",
        "SagaAssetPipelineLib",
        "Forge",
    };

    const auto offenders =
        FindForbiddenLinks(calls, "SagaEngine", forbidden);

    for (const auto& offender : offenders)
    {
        ADD_FAILURE()
            << "Forbidden target dependency: SagaEngine must not link "
            << JoinNames(FindForbiddenDependencyNames(offender, forbidden))
            << ". Offending call in " << path.generic_string()
            << ":" << offender.line << "\n" << offender.text;
    }
}

TEST(CMakeTargetBoundaryTests, SagaGraphicsTargetIsVendorNeutralPublicShell)
{
    const auto path = CMakeModulePath("SagaGraphicsTargets.cmake");
    const auto lines = ReadLines(path);
    const auto text = ReadText(path);
    const auto calls = ExtractTargetLinkCalls(lines);
    const auto engineCalls = ExtractTargetLinkCalls(ReadLines(SagaTargetsPath()));
    const auto rootText = ReadText(CMakeListsPath());

    EXPECT_TRUE(ContainsToken(rootText, "include(SagaGraphicsTargets)"))
        << "Root CMakeLists must load the graphics target boundary module.";
    EXPECT_TRUE(ContainsToken(rootText, "saga_create_graphics_targets()"))
        << "Root CMakeLists must create graphics targets before engine "
           "targets.";

    EXPECT_TRUE(ContainsToken(text, "add_library(SagaGraphics INTERFACE)"))
        << "SagaGraphics must exist as a public interface target.";
    EXPECT_TRUE(ContainsToken(text, "add_library(SagaGraphicsCore STATIC)"))
        << "SagaGraphicsCore must own vendor-neutral public implementation "
           "symbols.";
    EXPECT_TRUE(ContainsToken(text, "saga_get_graphics_core_sources"))
        << "Graphics core implementation sources must be defined in one "
           "shared CMake helper.";
    EXPECT_TRUE(ContainsToken(
        text,
        "target_sources(SagaGraphicsCore PRIVATE\n"
        "        ${SAGA_GRAPHICS_CORE_SOURCES}"))
        << "SagaGraphicsCore must compile the shared graphics core source "
           "list.";
    EXPECT_TRUE(ContainsToken(
        text,
        "target_include_directories(SagaGraphics INTERFACE\n"
        "        $<BUILD_INTERFACE:${SAGA_ROOT}/Engine/Public>\n"
        "        $<INSTALL_INTERFACE:include>"))
        << "SagaGraphics must expose only the public include root in build "
           "and install trees.";
    EXPECT_TRUE(ContainsToken(
        text,
        "target_include_directories(SagaGraphicsCore PUBLIC\n"
        "        $<BUILD_INTERFACE:${SAGA_ROOT}/Engine/Public>\n"
        "        $<INSTALL_INTERFACE:include>"))
        << "SagaGraphicsCore must expose only the public include root in "
           "build and install trees.";
    EXPECT_TRUE(ContainsToken(
        text,
        "target_link_libraries(SagaGraphics INTERFACE\n"
        "        SagaGraphicsCore"))
        << "SagaGraphics must expose vendor-neutral implementation symbols "
           "through SagaGraphicsCore.";

    const std::vector<std::string> forbidden = {
        "SagaGraphicsPrivate",
        "SagaDiligentBackend",
        "SagaDiligentRuntime",
        "VendorDiligent",
        "Diligent-",
        "Vulkan::",
        "D3D",
        "OpenGL",
        "Metal",
    };
    for (const auto& call : calls)
    {
        if (call.target != "SagaGraphics")
        {
            continue;
        }

        EXPECT_TRUE(ContainsToken(call.text, "SagaGraphicsCore"))
            << "SagaGraphics may only link its vendor-neutral core. "
            << "Offending call in " << path.generic_string() << ":"
            << call.line << "\n" << call.text;
        for (const auto& dependency : forbidden)
        {
            EXPECT_FALSE(ContainsToken(call.text, dependency))
                << "SagaGraphics must not link vendor, backend, native API, "
                   "or private graphics targets. Offending dependency: "
                << dependency << " in " << path.generic_string() << ":"
                << call.line << "\n" << call.text;
        }
    }

    bool sawEngineGraphicsLink = false;
    for (const auto& call : engineCalls)
    {
        if (call.target == "SagaEngine" &&
            ContainsToken(call.text, "SagaGraphics"))
        {
            sawEngineGraphicsLink = true;
        }
    }
    EXPECT_TRUE(sawEngineGraphicsLink)
        << "SagaEngine must consume the public SagaGraphics target instead "
           "of compiling public graphics implementation sources directly.";
}

TEST(CMakeTargetBoundaryTests, SagaGraphicsPrivateTargetIsPrivateBoundaryShell)
{
    const auto path = CMakeModulePath("SagaGraphicsTargets.cmake");
    const auto text = ReadText(path);
    const auto calls = ExtractTargetLinkCalls(ReadLines(path));

    EXPECT_TRUE(ContainsToken(text, "add_library(SagaGraphicsPrivate STATIC)"))
        << "SagaGraphicsPrivate must exist as a private implementation "
           "target shell.";
    EXPECT_TRUE(ContainsToken(
        text,
        "target_include_directories(SagaGraphicsPrivate PRIVATE\n"
        "        ${SAGA_ROOT}/Engine/Private"))
        << "SagaGraphicsPrivate must be allowed to include private engine "
           "headers.";
    EXPECT_TRUE(ContainsToken(
        text,
        "target_link_libraries(SagaGraphicsPrivate PUBLIC\n"
        "        SagaGraphics"))
        << "SagaGraphicsPrivate must depend on the public SagaGraphics shell.";
    EXPECT_TRUE(ContainsToken(
        text,
        "target_link_libraries(SagaGraphicsPrivate PRIVATE\n"
        "        SagaDiligentRuntime"))
        << "SagaGraphicsPrivate must consume the canonical private Diligent "
           "runtime target.";

    const std::vector<std::string> forbidden = {
        "VendorDiligent",
        "Diligent-",
        "Vulkan::",
        "D3D",
        "OpenGL",
        "Metal",
    };

    const auto offenders =
        FindForbiddenLinks(calls, "SagaGraphicsPrivate", forbidden);

    for (const auto& offender : offenders)
    {
        ADD_FAILURE()
            << "Forbidden target dependency: SagaGraphicsPrivate must not link "
            << JoinNames(FindForbiddenDependencyNames(offender, forbidden))
            << ". Offending call in " << path.generic_string()
            << ":" << offender.line << "\n" << offender.text;
    }
}

TEST(CMakeTargetBoundaryTests, DiligentBindingCompilerIsPrivateGraphicsOwned)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto compiler =
        root / "Engine" / "Private" / "SagaEngine" / "Graphics" /
        "Backends" / "Diligent" / "DiligentBindingCompiler.cpp";
    const auto records =
        root / "Engine" / "Private" / "SagaEngine" / "Graphics" /
        "Backends" / "Diligent" / "DiligentBindingRecords.h";
    const auto targetsText = ReadText(CMakeModulePath("SagaTargets.cmake"));
    const auto graphicsTargetsText =
        ReadText(CMakeModulePath("SagaGraphicsTargets.cmake"));

    ASSERT_TRUE(std::filesystem::exists(compiler));
    ASSERT_TRUE(std::filesystem::exists(records));
    EXPECT_TRUE(ContainsToken(
        compiler.generic_string(),
        "Engine/Private/SagaEngine/Graphics/Backends/Diligent"))
        << "Native binding compiler must stay inside the private Diligent "
           "graphics backend.";
    EXPECT_TRUE(ContainsToken(
        targetsText,
        "Engine/Private/SagaEngine/Graphics/Backends/Diligent/*.cpp"))
        << "Private Diligent graphics backend sources must be captured by "
           "the DiligentGraphicsRuntime source glob.";
    EXPECT_TRUE(ContainsToken(
        graphicsTargetsText,
        "saga_assert_diligent_graphics_backend_single_owner"))
        << "Diligent graphics backend sources must keep the single-owner "
           "configure assertion.";
}

TEST(CMakeTargetBoundaryTests, SagaGraphicsCoreStaysVendorNeutral)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const std::vector<std::filesystem::path> graphicsCoreFiles = {
        root / "Engine" / "Private" / "SagaEngine" / "Graphics" /
            "Backend" / "NullGraphicsBackend.cpp",
        root / "Engine" / "Private" / "SagaEngine" / "Graphics" /
            "Bindings" / "GraphicsBindingValidation.cpp",
    };
    const std::vector<std::string> forbiddenTokens = {
        "Diligent",
        "Vulkan",
        "Vk",
        "D3D",
        "Metal",
        "CreateShaderResourceBinding",
        "GetVariableByName",
    };

    for (const auto& file : graphicsCoreFiles)
    {
        const auto text = ReadText(file);
        for (const auto& token : forbiddenTokens)
        {
            EXPECT_FALSE(ContainsToken(text, token))
                << "SagaGraphicsCore source must stay vendor/native neutral: "
                << file.generic_string() << " contains " << token;
        }
    }
}

TEST(CMakeTargetBoundaryTests, DiligentBindingCompilerDoesNotUseSrbMutationApis)
{
    const auto compiler =
        std::filesystem::path(SAGA_SOURCE_ROOT) / "Engine" / "Private" /
        "SagaEngine" / "Graphics" / "Backends" / "Diligent" /
        "DiligentBindingCompiler.cpp";
    const auto text = ReadText(compiler);

    for (const auto& token : {
             "CreateShaderResourceBinding",
             "GetVariableByName",
             "IShaderResourceBinding",
             "ShaderResourceBinding",
         })
    {
        EXPECT_FALSE(ContainsToken(text, token))
            << "B1 compiler path must not create or mutate Diligent SRBs: "
            << token;
    }
}

TEST(CMakeTargetBoundaryTests, NativeBindingSrbApisAreCacheScoped)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto graphicsRoot = root / "Engine" / "Private" / "SagaEngine" /
                              "Graphics" / "Backends" / "Diligent";
    const auto compiler = ReadText(graphicsRoot / "DiligentBindingCompiler.cpp");
    const auto resolver = ReadText(graphicsRoot / "DiligentBindingResolver.cpp");
    const auto cache =
        ReadText(graphicsRoot / "Runtime" / "DiligentBindingCache.cpp");

    EXPECT_FALSE(ContainsToken(compiler, "CreateShaderResourceBinding"));
    EXPECT_FALSE(ContainsToken(compiler, "GetVariableByName"));
    EXPECT_FALSE(ContainsToken(compiler, "GetStaticVariableByName"));
    EXPECT_FALSE(ContainsToken(resolver, "CreateShaderResourceBinding"));
    EXPECT_FALSE(ContainsToken(resolver, "GetVariableByName"));
    EXPECT_FALSE(ContainsToken(resolver, "GetStaticVariableByName"));
    EXPECT_TRUE(ContainsToken(cache, "CreateShaderResourceBinding"));
    EXPECT_TRUE(ContainsToken(cache, "GetVariableByName"));
    EXPECT_TRUE(ContainsToken(cache, "GetStaticVariableByName"));

    const auto submit =
        ReadText(root / "Engine" / "Private" / "SagaEngine" / "Render" /
                 "Backend" / "Diligent" / "DiligentRenderBackendSubmit.cpp");
    EXPECT_FALSE(ContainsToken(submit, "DiligentBindingCache"));
    EXPECT_FALSE(ContainsToken(submit, "ResolveNativeBindingSrb"));
}

TEST(CMakeTargetBoundaryTests, DiligentFallbackResourcesArePrivateGraphicsOwned)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto graphicsRoot = root / "Engine" / "Private" / "SagaEngine" /
                              "Graphics" / "Backends" / "Diligent";
    const auto fallbackCpp = graphicsRoot / "DiligentFallbackResources.cpp";
    const auto fallbackHeader = graphicsRoot / "DiligentFallbackResources.h";
    const auto targetsText = ReadText(CMakeModulePath("SagaGraphicsTargets.cmake"));

    ASSERT_TRUE(std::filesystem::exists(fallbackCpp));
    ASSERT_TRUE(std::filesystem::exists(fallbackHeader));
    EXPECT_TRUE(ContainsToken(
        targetsText,
        "Engine/Private/SagaEngine/Graphics/Backends/Diligent/*.cpp"));
    EXPECT_TRUE(ContainsToken(
        targetsText,
        "saga_assert_diligent_graphics_backend_single_owner"));

    const auto compiler = ReadText(graphicsRoot / "DiligentBindingCompiler.cpp");
    const auto cache =
        ReadText(graphicsRoot / "Runtime" / "DiligentBindingCache.cpp");
    const auto fallback = ReadText(fallbackCpp);
    EXPECT_FALSE(ContainsToken(compiler, "DiligentFallbackResources"));
    EXPECT_FALSE(ContainsToken(cache, "DiligentFallbackResources"));
    EXPECT_TRUE(ContainsToken(fallback, "CreateTexture"));
    EXPECT_TRUE(ContainsToken(fallback, "CreateSampler"));
    EXPECT_FALSE(ContainsToken(fallback, "CreateShaderResourceBinding"));
    EXPECT_FALSE(ContainsToken(fallback, "GetVariableByName"));
    EXPECT_FALSE(ContainsToken(fallback, "CreateDevice"));
    EXPECT_FALSE(ContainsToken(fallback, "CreateSwapChain"));
}

TEST(CMakeTargetBoundaryTests, DiligentBindingCompilerDoesNotMigrateDrawOwnership)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const std::vector<std::filesystem::path> files = {
        root / "Engine" / "Private" / "SagaEngine" / "Graphics" /
            "Backends" / "Diligent" / "DiligentBindingCompiler.cpp",
        root / "Engine" / "Private" / "SagaEngine" / "Graphics" /
            "Backends" / "Diligent" / "DiligentBindingCompiler.h",
        root / "Engine" / "Private" / "SagaEngine" / "Graphics" /
            "Backends" / "Diligent" / "DiligentBindingRecords.h",
    };
    const std::vector<std::string> forbiddenTokens = {
        "DiligentRenderBackendSubmit",
        "DiligentRenderBackendResources",
        "CreateMaterial",
        "Submit(",
        "DrawIndexed",
        "shadowVariable",
        "skinnedSrb",
    };

    for (const auto& file : files)
    {
        const auto text = ReadText(file);
        for (const auto& token : forbiddenTokens)
        {
            EXPECT_FALSE(ContainsToken(text, token))
                << "B1 must not migrate submit/material/shadow ownership into "
                   "the binding compiler: "
                << file.generic_string() << " contains " << token;
        }
    }
}

TEST(CMakeTargetBoundaryTests, GraphicsInstallSurfaceDoesNotInstallVendorBackends)
{
    const auto installPath = CMakeModulePath("SagaInstall.cmake");
    const auto graphicsInstallPath = CMakeModulePath("SagaInstallGraphics.cmake");
    const auto vendorPath = CMakeModulePath("SagaDiligentVendor.cmake");
    ASSERT_TRUE(std::filesystem::exists(installPath));
    ASSERT_TRUE(std::filesystem::exists(graphicsInstallPath));
    ASSERT_TRUE(std::filesystem::exists(vendorPath));

    const auto installText = ReadText(installPath);
    const auto graphicsInstallText = ReadText(graphicsInstallPath);
    const auto rootText = ReadText(CMakeListsPath());
    EXPECT_TRUE(ContainsToken(rootText, "include(SagaInstallGraphics)"))
        << "Root CMakeLists must load the graphics install boundary module.";
    EXPECT_TRUE(ContainsToken(installText, "saga_setup_graphics_install()"))
        << "Main install setup must delegate graphics headers to the "
           "graphics install module.";
    EXPECT_TRUE(ContainsToken(
        installText,
        "install(DIRECTORY \"${SAGA_ROOT}/Engine/Public/SagaEngine\""))
        << "SagaEngine public include tree must remain the installed "
           "development include surface.";
    EXPECT_TRUE(ContainsToken(installText, "PATTERN \"Graphics\" EXCLUDE"))
        << "Main public include install must leave Graphics headers to the "
           "graphics install module.";
    EXPECT_TRUE(ContainsToken(
        graphicsInstallText,
        "install(DIRECTORY \"${SAGA_ROOT}/Engine/Public/SagaEngine/Graphics\""))
        << "Graphics public headers must have an explicit install rule.";
    EXPECT_TRUE(ContainsToken(
        graphicsInstallText,
        "DESTINATION \"${CMAKE_INSTALL_INCLUDEDIR}/SagaEngine\""))
        << "Graphics headers must install below include/SagaEngine.";
    EXPECT_TRUE(ContainsToken(graphicsInstallText, "SagaGraphicsCore"))
        << "SagaGraphicsCore must be installed as the public implementation "
           "dependency for SagaGraphics.";
    EXPECT_TRUE(ContainsToken(graphicsInstallText, "SagaGraphics"))
        << "SagaGraphics must be installed as the public consumer target.";
    EXPECT_TRUE(ContainsToken(graphicsInstallText, "EXPORT SagaEngineTargets"))
        << "Graphics targets must use the package export set.";
    EXPECT_TRUE(ContainsToken(graphicsInstallText, "NAMESPACE SagaEngine::"))
        << "Installed graphics targets must use the SagaEngine:: namespace.";
    EXPECT_TRUE(ContainsToken(
        graphicsInstallText,
        "SagaEngineConfig.cmake"))
        << "Installed graphics consumers must have a package config.";

    const std::vector<std::string> forbiddenInstallTokens = {
        "VendorDiligent",
        "SagaDiligentBackend",
        "SagaDiligentRuntime",
        "SagaGraphicsPrivate",
        "Diligent-",
        "Vulkan::",
        "D3D",
        "OpenGL",
        "Metal",
    };

    const std::string combinedInstallText =
        installText + "\n" + graphicsInstallText;
    for (const auto& token : forbiddenInstallTokens)
    {
        EXPECT_FALSE(ContainsToken(combinedInstallText, token))
            << "Install rules must not publish graphics backend/vendor token: "
            << token;
    }

    EXPECT_FALSE(ContainsToken(installText, "Vendor/Diligent"))
        << "Install rules must not copy vendored graphics sources.";
    EXPECT_FALSE(ContainsToken(installText, "VendorDiligent"))
        << "VendorDiligent must not be installed as a public target.";
    EXPECT_FALSE(ContainsToken(installText, "SagaDiligentBackend"))
        << "Concrete Diligent backend target must not be installed.";
    EXPECT_FALSE(ContainsToken(installText, "SagaDiligentRuntime"))
        << "Private Diligent runtime target must not be installed.";
    EXPECT_FALSE(ContainsToken(installText, "SagaGraphicsPrivate"))
        << "Private graphics implementation target must not be installed.";

    const auto vendorText = ReadText(vendorPath);
    EXPECT_TRUE(ContainsToken(vendorText, "DILIGENT_INSTALL_CORE OFF"));
    EXPECT_TRUE(ContainsToken(vendorText, "DILIGENT_INSTALL_FX OFF"));
    EXPECT_TRUE(ContainsToken(vendorText, "DILIGENT_INSTALL_TOOLS OFF"));
    EXPECT_TRUE(ContainsToken(vendorText, "DILIGENT_INSTALL_SAMPLES OFF"));
}

TEST(CMakeTargetBoundaryTests, SagaDiagnosticsDoesNotLinkRuntimeServerEditorOrToolTargets)
{
    const auto path = SagaTargetsPath();
    const auto calls = ExtractTargetLinkCalls(ReadLines(path));
    const std::vector<std::string> forbidden = {
        "SagaRuntimeLib",
        "SagaServerLib",
        "SagaProductLib",
        "SagaEditorLib",
        "SagaEditorLabLib",
        "SagaEditorLabBridge",
        "SagaAssetPipelineLib",
        "Forge",
    };

    const auto offenders =
        FindForbiddenLinks(calls, "SagaDiagnostics", forbidden);

    for (const auto& offender : offenders)
    {
        ADD_FAILURE()
            << "Forbidden target dependency: SagaDiagnostics must not link "
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
        "SagaSandboxLib",
        "SagaProductLib",
        "SagaAssetPipelineLib",
        "Forge",
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

TEST(CMakeTargetBoundaryTests, SagaRuntimeOwnsOnlyRuntimeAppSourcesAndIncludes)
{
    const auto path = SagaTargetsPath();
    const std::string text = ReadText(path);
    const std::string sources =
        ExtractTargetCall(text, "add_executable", "SagaRuntime");
    const std::string includes =
        ExtractTargetCall(text, "target_include_directories", "SagaRuntime");

    ASSERT_FALSE(sources.empty())
        << "SagaRuntime add_executable call not found in "
        << path.generic_string();

    const std::vector<std::string> forbiddenSourceRoots = {
        "Apps/Client",
        "Apps/Server",
        "Apps/Saga/",
        "Apps/Editor/",
        "Apps/EditorLab",
        "Apps/Sandbox",
        "Apps/SagaDev",
    };
    for (const auto& forbidden : forbiddenSourceRoots)
    {
        EXPECT_FALSE(ContainsToken(sources, forbidden))
            << "SagaRuntime must not compile app implementation from "
            << forbidden << ". Offending call:\n" << sources;
    }

    EXPECT_FALSE(ContainsToken(includes, "Apps/Client"))
        << "SagaRuntime must not include Apps/Client. Offending call:\n"
        << includes;
    EXPECT_FALSE(ContainsToken(includes, "Apps/Server"))
        << "SagaRuntime must not include Apps/Server. Offending call:\n"
        << includes;
}

TEST(CMakeTargetBoundaryTests, DedicatedServerTargetsStayHeadless)
{
    const auto path = SagaTargetsPath();
    const auto lines = ReadLines(path);
    const auto calls = ExtractTargetLinkCalls(lines);

    const std::vector<std::string> serverTargets = {
        "SagaServerLib",
        "SagaServer",
    };
    const std::vector<std::string> forbiddenDependencies = {
        "SagaPlatformSDL",
        "SagaBackend",
        "SDL2::",
        "imgui::",
        "Qt6::",
        "rmlui::",
        "RmlUi",
        "Diligent",
        "libpqxx::",
        "pqxx::",
        "hiredis::",
        "redis++::",
    };

    for (const auto& target : serverTargets)
    {
        const auto offenders =
            FindForbiddenLinks(calls, target, forbiddenDependencies);

        for (const auto& offender : offenders)
        {
            ADD_FAILURE()
                << "Headless server target " << target
                << " must not link "
                << JoinNames(FindForbiddenDependencyNames(
                       offender,
                       forbiddenDependencies))
                << ". Offending call in " << path.generic_string()
                << ":" << offender.line << "\n" << offender.text;
        }

        const std::string blanketCall =
            "saga_link_thirdparty(" + target + ")";

        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            if (ContainsToken(lines[i], blanketCall))
            {
                ADD_FAILURE()
                    << "Headless server target " << target
                    << " must use explicit dependencies instead of "
                    << blanketCall << ". Offending line in "
                    << path.generic_string() << ":" << (i + 1)
                    << "\n" << lines[i];
            }
        }
    }
}

TEST(CMakeTargetBoundaryTests, SDLPlatformSourcesStayOutsideSagaEngine)
{
    const auto path = SagaTargetsPath();
    const auto text = ReadText(path);
    const auto calls = ExtractTargetLinkCalls(ReadLines(path));

    EXPECT_TRUE(ContainsToken(
        text,
        "set(SAGA_PLATFORM_SDL_SOURCES"))
        << "SDL implementation sources must have an explicit source group.";

    for (const auto& source : {
             "Input/Backends/SDL/SDLInputBackend.cpp",
             "Platform/SDL/SDLDebugRenderer2D.cpp",
             "Platform/SDL/SDLPlatformFactory.cpp",
             "Platform/SDL/SDLWindow.cpp",
         })
    {
        EXPECT_TRUE(ContainsToken(text, source))
            << "SagaPlatformSDL source list is missing " << source;
    }

    EXPECT_TRUE(ContainsToken(
        text,
        "list(REMOVE_ITEM ENGINE_SOURCES ${SAGA_PLATFORM_SDL_SOURCES})"))
        << "SDL implementation sources must be removed from SagaEngine.";

    EXPECT_TRUE(ContainsToken(
        text,
        "target_sources(SagaPlatformSDL PRIVATE"))
        << "SagaPlatformSDL must own the SDL implementation sources.";

    bool sawSDLTargetLink = false;
    for (const auto& call : calls)
    {
        if (call.target != "SagaPlatformSDL")
        {
            continue;
        }

        sawSDLTargetLink = true;
        EXPECT_TRUE(ContainsToken(call.text, "SagaEngine"))
            << "SagaPlatformSDL must build on the vendor-neutral engine.";
        EXPECT_TRUE(ContainsToken(call.text, "SDL2::SDL2"))
            << "SagaPlatformSDL must explicitly own the SDL2 dependency.";
    }

    EXPECT_TRUE(sawSDLTargetLink)
        << "Expected a target_link_libraries call for SagaPlatformSDL.";

    const auto engineOffenders = FindForbiddenLinks(
        calls,
        "SagaEngine",
        {
            "SagaPlatformSDL",
            "SDL2::",
            "imgui::",
            "libpqxx::",
            "hiredis::",
            "redis++::",
        });

    for (const auto& offender : engineOffenders)
    {
        ADD_FAILURE()
            << "SagaEngine must remain free of concrete SDL, UI, and "
               "persistence dependencies. Offending call in "
            << path.generic_string() << ":" << offender.line
            << "\n" << offender.text;
    }
}

TEST(CMakeTargetBoundaryTests, RmlUiIsLinkedOnlyBySagaBackend)
{
    const auto path = SagaTargetsPath();
    const auto lines = ReadLines(path);

    bool sawBackendLink = false;
    std::vector<std::string> offenders;
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const auto& line = lines[i];
        if (line.find("saga_link_rmlui(") == std::string::npos)
        {
            continue;
        }

        if (line.find("SagaBackend") != std::string::npos)
        {
            sawBackendLink = true;
            continue;
        }

        offenders.push_back(
            path.generic_string() + ":" + std::to_string(i + 1) + ": " + line);
    }

    EXPECT_TRUE(sawBackendLink)
        << "SagaBackend must own the RmlUi adapter link dependency.";
    EXPECT_TRUE(offenders.empty())
        << "RmlUi must not be linked outside SagaBackend. First offender: "
        << (offenders.empty() ? "" : offenders.front());
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

TEST(CMakeTargetBoundaryTests, EditorCustomizationCoreStaysQtAndToolFree)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const std::vector<std::filesystem::path> files = {
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "EditorCustomizationCapability.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "EditorCustomizationDiagnostics.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "EditorCustomizationOverlay.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "EditorCustomizationOverlayLoader.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "ShortcutCustomizationController.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "ShortcutCustomizationFeedback.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "ShortcutCustomizationModel.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "ShortcutCustomizationPanelViewModel.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "ShortcutCustomizationSession.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "WorkspaceCustomizationController.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "WorkspaceCustomizationModel.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "WorkspaceCustomizationOverlayPolicy.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "WorkspaceCustomizationOverlayStore.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "WorkspaceCustomizationFeedback.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "WorkspaceCustomizationPanelViewModel.h",
        root / "Editor" / "include" / "SagaEditor" / "Customization" / "WorkspaceCustomizationSession.h",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "EditorCustomizationCapability.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "EditorCustomizationDiagnostics.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "EditorCustomizationOverlayLoader.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "ShortcutCustomizationController.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "ShortcutCustomizationFeedback.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "ShortcutCustomizationModel.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "ShortcutCustomizationPanelViewModel.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "ShortcutCustomizationSession.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "WorkspaceCustomizationController.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "WorkspaceCustomizationModel.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "WorkspaceCustomizationOverlayPolicy.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "WorkspaceCustomizationOverlayStore.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "WorkspaceCustomizationFeedback.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "WorkspaceCustomizationPanelViewModel.cpp",
        root / "Editor" / "src" / "SagaEditor" / "Customization" / "WorkspaceCustomizationSession.cpp",
    };
    const std::vector<std::string> forbiddenTokens = {
        "QApplication",
        "QWidget",
        "Qt6::",
        "#include <Q",
        "Forge/",
    };
    std::vector<std::string> offenders;
    for (const auto& file : files)
    {
        const std::string text = ReadText(file);
        for (const std::string& token : forbiddenTokens)
        {
            if (text.find(token) != std::string::npos)
            {
                offenders.push_back(file.generic_string() + ": " + token);
                break;
            }
        }
    }

    EXPECT_TRUE(offenders.empty())
        << "Safe customization core must stay Qt-free and tool-free. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(CMakeTargetBoundaryTests, CustomizeWorkspacePanelDoesNotInvokeToolchain)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto offenders = FindForbiddenText(
        root / "Editor" / "src" / "SagaEditor" / "UI" / "Qt" / "Panels",
        {
            "Forge/",
        });

    EXPECT_TRUE(offenders.empty())
        << "Qt editor panels must not invoke or include composition toolchain internals. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());
}

TEST(CMakeTargetBoundaryTests, AppsEditorDoesNotInvokeCompositionToolchain)
{
    const auto root = std::filesystem::path(SAGA_SOURCE_ROOT);
    const auto offenders = FindForbiddenText(
        root / "Apps" / "Editor",
        {
            "Forge/",
        });

    EXPECT_TRUE(offenders.empty())
        << "Apps/Editor may consume composition manifests but must not invoke the toolchain. "
        << "First offender: "
        << (offenders.empty() ? "" : offenders.front());
}
