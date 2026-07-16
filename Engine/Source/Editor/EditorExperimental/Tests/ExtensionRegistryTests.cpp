// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Extensions/ExtensionRegistry.h"
#include "SagaEditor/Extensions/IEditorExtension.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace SagaEditor {
namespace {

class TestExtension final : public IEditorExtension
{
public:
    TestExtension(std::string id, std::string name)
        : m_id(std::move(id)), m_name(std::move(name)) {}

    std::string GetExtensionId() const override { return m_id; }
    std::string GetDisplayName() const override { return m_name; }
    std::string GetVersion() const override { return "1.0.0"; }
    void OnLoad(IExtensionContext&) override {}
    void OnUnload(IExtensionContext&) override {}

private:
    std::string m_id;
    std::string m_name;
};

TEST(ExtensionRegistryTests, PreservesInsertionOrderAndSupportsLookup)
{
    ExtensionRegistry registry;
    registry.Register(std::make_unique<TestExtension>("tools.first", "First"));
    registry.Register(std::make_unique<TestExtension>("tools.second", "Second"));

    ASSERT_NE(registry.Find("tools.first"), nullptr);
    EXPECT_EQ(registry.Find("tools.first")->GetDisplayName(), "First");
    EXPECT_EQ(registry.Find("missing"), nullptr);

    const auto all = registry.GetAll();
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0]->GetExtensionId(), "tools.first");
    EXPECT_EQ(all[1]->GetExtensionId(), "tools.second");
}

TEST(ExtensionRegistryTests, ReplacementDoesNotDuplicateOrderEntry)
{
    ExtensionRegistry registry;
    registry.Register(std::make_unique<TestExtension>("tools.same", "Old"));
    registry.Register(std::make_unique<TestExtension>("tools.same", "New"));

    const auto all = registry.GetAll();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all.front()->GetDisplayName(), "New");

    registry.Unregister("tools.same");
    EXPECT_TRUE(registry.GetAll().empty());
    EXPECT_EQ(registry.Find("tools.same"), nullptr);
}

} // namespace
} // namespace SagaEditor
