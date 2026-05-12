/// @file JsonModelLoaderTests.cpp
/// @brief Tests for JsonModelLoader: file I/O errors, format validation, type mapping.

#include "SDE/IO/JsonModelLoader.h"
#include "SDE/IO/ModelLoader.h"
#include "SDE/Model/TypeNode.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

using namespace SDE;
namespace fs = std::filesystem;

namespace
{

/// Write content to a temp file and return its path.
std::string WriteTempFile(const std::string& content, const std::string& suffix = ".json")
{
    fs::path p = fs::temp_directory_path() / ("sde_test_" + std::to_string(
        std::hash<std::string>{}(content)) + suffix);
    std::ofstream out(p);
    out << content;
    return p.string();
}

} // namespace

// ─── Missing file ─────────────────────────────────────────────────────────────

TEST(JsonModelLoaderTest, MissingFile_EmitsFatalDiagnostic)
{
    JsonModelLoader      loader;
    std::vector<Diagnostic> diags;
    auto instances = loader.Load("/nonexistent/path/does_not_exist.json", diags);

    EXPECT_TRUE(instances.empty());
    ASSERT_EQ(diags.size(), 1u);
    EXPECT_EQ(diags[0].severity, Severity::Fatal);
    EXPECT_EQ(diags[0].code,     "SDE_IO_ERROR");
}

// ─── Valid data file ──────────────────────────────────────────────────────────

TEST(JsonModelLoaderTest, ValidArrayFile_ReturnsTwoInstances)
{
    const std::string json = R"({
        "modelId": "Item",
        "data": [
            { "id": "sword_01", "name": "Sword" },
            { "id": "shield_01", "name": "Shield" }
        ]
    })";
    std::string path = WriteTempFile(json);

    JsonModelLoader      loader;
    std::vector<Diagnostic> diags;
    auto instances = loader.Load(path, diags);

    EXPECT_TRUE(diags.empty());
    ASSERT_EQ(instances.size(), 2u);
    EXPECT_EQ(instances[0].modelId,    "Item");
    EXPECT_EQ(instances[0].instanceId, "sword_01");
    EXPECT_EQ(instances[1].instanceId, "shield_01");
}

TEST(JsonModelLoaderTest, InstanceModelIdMatchesFileModelId)
{
    const std::string json = R"({
        "modelId": "Skill",
        "data": [ { "id": "slash" } ]
    })";
    std::string path = WriteTempFile(json, "_skill.json");

    JsonModelLoader      loader;
    std::vector<Diagnostic> diags;
    auto instances = loader.Load(path, diags);

    ASSERT_EQ(instances.size(), 1u);
    EXPECT_EQ(instances[0].modelId, "Skill");
}

// ─── Integer vs Number distinction ────────────────────────────────────────────

TEST(JsonModelLoaderTest, IntegerField_IsRawInteger)
{
    const std::string json = R"({
        "modelId": "Item",
        "data": [ { "id": "x", "level": 5 } ]
    })";
    std::string path = WriteTempFile(json, "_int.json");

    JsonModelLoader      loader;
    std::vector<Diagnostic> diags;
    auto instances = loader.Load(path, diags);

    ASSERT_EQ(instances.size(), 1u);
    const RawValue& level = instances[0].fields.at("level");
    EXPECT_TRUE(level.IsInteger());
    EXPECT_FALSE(level.IsNumber() && !level.IsInteger()); // IsNumber() true, but it IS integer
    EXPECT_EQ(level.AsInteger(), 5);
}

TEST(JsonModelLoaderTest, FloatField_IsRawNumberNotInteger)
{
    const std::string json = R"({
        "modelId": "Item",
        "data": [ { "id": "x", "weight": 1.5 } ]
    })";
    std::string path = WriteTempFile(json, "_float.json");

    JsonModelLoader      loader;
    std::vector<Diagnostic> diags;
    auto instances = loader.Load(path, diags);

    ASSERT_EQ(instances.size(), 1u);
    const RawValue& weight = instances[0].fields.at("weight");
    EXPECT_FALSE(weight.IsInteger());
    EXPECT_TRUE(weight.IsNumber());
}

// ─── Parse errors ─────────────────────────────────────────────────────────────

TEST(JsonModelLoaderTest, MalformedJson_EmitsFatalParseError)
{
    std::string path = WriteTempFile("{ this is not json }", "_bad.json");

    JsonModelLoader      loader;
    std::vector<Diagnostic> diags;
    auto instances = loader.Load(path, diags);

    EXPECT_TRUE(instances.empty());
    ASSERT_GE(diags.size(), 1u);
    EXPECT_EQ(diags[0].severity, Severity::Fatal);
    EXPECT_EQ(diags[0].code,     "SDE_PARSE_ERROR");
}

TEST(JsonModelLoaderTest, MissingModelIdKey_EmitsFatalBadFormat)
{
    std::string path = WriteTempFile(R"({ "data": [] })", "_nomodel.json");

    JsonModelLoader      loader;
    std::vector<Diagnostic> diags;
    auto instances = loader.Load(path, diags);

    EXPECT_TRUE(instances.empty());
    ASSERT_GE(diags.size(), 1u);
    EXPECT_EQ(diags[0].code, "SDE_BAD_FORMAT");
}

TEST(JsonModelLoaderTest, MissingDataKey_EmitsFatalBadFormat)
{
    std::string path = WriteTempFile(R"({ "modelId": "Item" })", "_nodata.json");

    JsonModelLoader      loader;
    std::vector<Diagnostic> diags;
    auto instances = loader.Load(path, diags);

    EXPECT_TRUE(instances.empty());
    ASSERT_GE(diags.size(), 1u);
    EXPECT_EQ(diags[0].code, "SDE_BAD_FORMAT");
}

// ─── LoadDefinition ───────────────────────────────────────────────────────────

TEST(JsonModelLoaderTest, LoadDefinition_ParsesFieldTypeText)
{
    const std::string json = R"({
        "id": "Item",
        "displayName": "Game Item",
        "schemaVersion": 1,
        "fields": [
            { "id": "name", "type": "Text", "presence": "required" }
        ]
    })";
    std::string path = WriteTempFile(json, "_schema.json");

    TypeRegistry        types;
    std::vector<Diagnostic> diags;
    auto defOpt = JsonModelLoader::LoadDefinition(path, types, diags);

    ASSERT_TRUE(defOpt.has_value());
    EXPECT_EQ(defOpt->id, "Item");
    ASSERT_EQ(defOpt->fields.size(), 1u);
    EXPECT_EQ(defOpt->fields[0].id, "name");

    const TypeNode& tn = types.Get(defOpt->fields[0].type);
    EXPECT_EQ(tn.kind, TypeKind::Text);
}

TEST(JsonModelLoaderTest, LoadDefinition_MissingFile_ReturnsNullopt)
{
    TypeRegistry        types;
    std::vector<Diagnostic> diags;
    auto defOpt = JsonModelLoader::LoadDefinition("/no/such/schema.json", types, diags);

    EXPECT_FALSE(defOpt.has_value());
    ASSERT_GE(diags.size(), 1u);
    EXPECT_EQ(diags[0].severity, Severity::Fatal);
}
