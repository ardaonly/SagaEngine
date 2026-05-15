/// @file CompilerSessionTests.cpp
/// @brief Tests for project-level compiler lifecycle contracts.

#include "SDE/Compiler/CompilerSession.h"
#include "SDE/Core/Cancellation.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace SDE;
namespace fs = std::filesystem;

namespace
{

fs::path MakeProjectDir(const std::string& name)
{
    fs::path root = fs::temp_directory_path() / name;
    fs::remove_all(root);
    fs::create_directories(root / "schemas");
    fs::create_directories(root / "data");
    return root;
}

fs::path MakeNativeProjectDir(const std::string& name)
{
    fs::path root = fs::temp_directory_path() / name;
    fs::remove_all(root);
    fs::create_directories(root / "source");
    return root;
}

void WriteFile(const fs::path& path, const std::string& text)
{
    std::ofstream out(path);
    out << text;
}

} // namespace

TEST(CompilerSessionTest, CancelBeforeLoadPublishesNoGraph)
{
    fs::path root = MakeProjectDir("sde_cancelled_project");
    CancellationSource source;
    source.RequestCancellation();

    CompilerSession session({root});
    CompileContext context;
    context.cancellation = source.Token();

    auto result = session.Compile(context);

    EXPECT_TRUE(result.cancelled);
    EXPECT_EQ(result.state, CompileState::Aborted);
    EXPECT_FALSE(result.graph.has_value());
    ASSERT_EQ(result.validation.diagnostics.size(), 1u);
    EXPECT_EQ(result.validation.diagnostics[0].code, "SDE_CANCELLED");
}

TEST(CompilerSessionTest, ImportCycleIsRejected)
{
    fs::path root = MakeProjectDir("sde_import_cycle_project");
    WriteFile(root / "schemas" / "a.json", R"({
        "id": "A",
        "schemaVersion": 1,
        "imports": [ "B" ],
        "fields": [ { "id": "name", "type": "Text" } ]
    })");
    WriteFile(root / "schemas" / "b.json", R"({
        "id": "B",
        "schemaVersion": 1,
        "imports": [ "A" ],
        "fields": [ { "id": "name", "type": "Text" } ]
    })");

    CompilerSession session({root});
    auto result = session.Compile();

    EXPECT_EQ(result.state, CompileState::ValidationFailed);
    ASSERT_FALSE(result.validation.diagnostics.empty());
    EXPECT_EQ(result.validation.diagnostics[0].code, "SDE_IMPORT_CYCLE");
    EXPECT_FALSE(result.graph.has_value());
}

TEST(CompilerSessionTest, NativeSdeSourceCompilesWithLanguageAndModelVersions)
{
    fs::path root = MakeNativeProjectDir("sde_native_source_project");
    WriteFile(root / "source" / "profiles.sde", R"(sde version 1

package Saga.Editor.Customization

model EditorProfile version 1 {
  field displayName Text required
  field defaultPanels List<Text> optional
  field showStatusBar Boolean optional
}

instance EditorProfile saga.profile.project {
  displayName = "Project"
  defaultPanels = ["Hierarchy", "Inspector"]
  showStatusBar = true
}
)");

    CompilerSession session({root});
    auto result = session.Compile();

    ASSERT_TRUE(IsUsable(result.state));
    ASSERT_TRUE(result.graph.has_value());
    EXPECT_EQ(result.versions.language.major, 1u);
    EXPECT_FALSE(result.hashes.sourceHash.empty());
    EXPECT_FALSE(result.hashes.artifactHash.empty());
    EXPECT_TRUE(result.graph->Find("EditorProfile", "saga.profile.project") != nullptr);
}

TEST(CompilerSessionTest, NativeSdeSourceRequiresLanguageVersion)
{
    fs::path root = MakeNativeProjectDir("sde_native_source_missing_version");
    WriteFile(root / "source" / "broken.sde", R"(package Saga.Editor.Customization)");

    CompilerSession session({root});
    auto result = session.Compile();

    EXPECT_EQ(result.state, CompileState::ValidationFailed);
    ASSERT_FALSE(result.validation.diagnostics.empty());
    EXPECT_EQ(result.validation.diagnostics.front().code, "SDE_EXPECTED_KEYWORD");
}

TEST(CompilerSessionTest, NativeSdeSourceRejectsDuplicateInstances)
{
    fs::path root = MakeNativeProjectDir("sde_native_source_duplicate_instances");
    WriteFile(root / "source" / "profiles.sde", R"(sde version 1

package Saga.Editor.Customization

model EditorProfile version 1 {
  field displayName Text required
}

instance EditorProfile saga.profile.project {
  displayName = "One"
}

instance EditorProfile saga.profile.project {
  displayName = "Two"
}
)");

    CompilerSession session({root});
    auto result = session.Compile();

    EXPECT_EQ(result.state, CompileState::ValidationFailed);
    bool found = false;
    for (const auto& diagnostic : result.validation.diagnostics)
        found = found || diagnostic.code == "SDE_DUPLICATE_INSTANCE";
    EXPECT_TRUE(found);
}
