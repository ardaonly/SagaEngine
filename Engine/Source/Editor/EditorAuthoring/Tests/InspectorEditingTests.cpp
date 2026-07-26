// SPDX-License-Identifier: MPL-2.0
/// @file InspectorEditingTests.cpp
/// @brief GoogleTest coverage for the InspectorEditing primitives.
///
/// Targets `ComponentEditorRegistry` and `InspectorBinder`. These types
/// form the framework-free core of the active inspector path, so unit
/// coverage runs in the engine's existing GoogleTest harness without
/// linking the editor's UI backend.

#include "SagaEditor/InspectorEditing/ComponentEditorRegistry.h"
#include "SagaEditor/InspectorEditing/InspectorBinder.h"
#include "SagaEditor/Panels/IPanel.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <typeindex>
#include <utility>
#include <vector>

namespace
{

using namespace SagaEditor;

// ─── Test Fixtures ────────────────────────────────────────────────────────────

/// Trivial component types used as registry keys. The bodies do not
/// matter — only their `std::type_index` is observed.
struct TransformLike { float x{}, y{}, z{}; };
struct HealthLike    { int  current{}, max{}; };
struct TagLike       { std::string label; };

/// Minimal `IPanel` implementation so the registry's factory can hand
/// back a real owned object without dragging in a Qt widget.
class StubPanel : public IPanel
{
public:
    explicit StubPanel(std::string title) : m_title(std::move(title)) {}

    [[nodiscard]] PanelId     GetPanelId()      const override { return "saga.test.stub"; }
    [[nodiscard]] std::string GetTitle()        const override { return m_title; }
    [[nodiscard]] QWidget*    Widget() const noexcept override { return nullptr; }

private:
    std::string m_title;
};

// ─── ComponentEditorRegistry ──────────────────────────────────────────────────

TEST(ComponentEditorRegistryTest, NewRegistryIsEmpty)
{
    ComponentEditorRegistry reg;
    EXPECT_EQ(reg.Size(), 0u);
    EXPECT_FALSE(reg.Has(std::type_index(typeid(TransformLike))));
    EXPECT_EQ(reg.Find(std::type_index(typeid(TransformLike))), nullptr);
    EXPECT_EQ(reg.Create(std::type_index(typeid(TransformLike))), nullptr);
}

TEST(ComponentEditorRegistryTest, RegisterMakesEditorDiscoverable)
{
    ComponentEditorRegistry reg;
    reg.Register<TransformLike>("Transform",
        [] { return std::make_unique<StubPanel>("Transform"); });

    EXPECT_TRUE(reg.Has(std::type_index(typeid(TransformLike))));
    EXPECT_EQ(reg.Size(), 1u);

    const auto* desc = reg.Find(std::type_index(typeid(TransformLike)));
    ASSERT_NE(desc, nullptr);
    EXPECT_EQ(desc->displayName, "Transform");
    EXPECT_EQ(desc->componentType, std::type_index(typeid(TransformLike)));
}

TEST(ComponentEditorRegistryTest, CreateReturnsFreshPanelEachCall)
{
    ComponentEditorRegistry reg;
    reg.Register<HealthLike>("Health",
        [] { return std::make_unique<StubPanel>("Health"); });

    auto first  = reg.Create(std::type_index(typeid(HealthLike)));
    auto second = reg.Create(std::type_index(typeid(HealthLike)));

    ASSERT_NE(first.get(),  nullptr);
    ASSERT_NE(second.get(), nullptr);
    EXPECT_NE(first.get(), second.get());
    EXPECT_EQ(first->GetTitle(),  "Health");
    EXPECT_EQ(second->GetTitle(), "Health");
}

TEST(ComponentEditorRegistryTest, ReregisterReplacesPreviousFactory)
{
    ComponentEditorRegistry reg;
    reg.Register<TagLike>("Tag",
        [] { return std::make_unique<StubPanel>("Tag-v1"); });
    reg.Register<TagLike>("Tag (v2)",
        [] { return std::make_unique<StubPanel>("Tag-v2"); });

    EXPECT_EQ(reg.Size(), 1u);
    const auto* desc = reg.Find(std::type_index(typeid(TagLike)));
    ASSERT_NE(desc, nullptr);
    EXPECT_EQ(desc->displayName, "Tag (v2)");

    auto panel = reg.Create(std::type_index(typeid(TagLike)));
    ASSERT_NE(panel.get(), nullptr);
    EXPECT_EQ(panel->GetTitle(), "Tag-v2");
}

TEST(ComponentEditorRegistryTest, UnregisterDropsEntry)
{
    ComponentEditorRegistry reg;
    reg.Register<TagLike>("Tag",
        [] { return std::make_unique<StubPanel>("Tag"); });

    EXPECT_TRUE(reg.Unregister(std::type_index(typeid(TagLike))));
    EXPECT_FALSE(reg.Has(std::type_index(typeid(TagLike))));
    EXPECT_FALSE(reg.Unregister(std::type_index(typeid(TagLike))));
}

TEST(ComponentEditorRegistryTest, ClearDropsEverything)
{
    ComponentEditorRegistry reg;
    reg.Register<TransformLike>("Transform",
        [] { return std::make_unique<StubPanel>("Transform"); });
    reg.Register<HealthLike>("Health",
        [] { return std::make_unique<StubPanel>("Health"); });
    EXPECT_EQ(reg.Size(), 2u);

    reg.Clear();
    EXPECT_EQ(reg.Size(), 0u);
    EXPECT_FALSE(reg.Has(std::type_index(typeid(TransformLike))));
    EXPECT_FALSE(reg.Has(std::type_index(typeid(HealthLike))));
}

TEST(ComponentEditorRegistryTest, FactoryReturningNullIsForwarded)
{
    ComponentEditorRegistry reg;
    reg.Register<TransformLike>("Transform",
        [] { return std::unique_ptr<IPanel>{}; });

    // Has is true (the factory is registered) but Create yields nullptr.
    EXPECT_TRUE(reg.Has(std::type_index(typeid(TransformLike))));
    EXPECT_EQ(reg.Create(std::type_index(typeid(TransformLike))), nullptr);
}

TEST(ComponentEditorRegistryTest, DescriptorsSnapshotMatchesRegistry)
{
    ComponentEditorRegistry reg;
    reg.Register<TransformLike>("Transform",
        [] { return std::make_unique<StubPanel>("Transform"); });
    reg.Register<HealthLike>("Health",
        [] { return std::make_unique<StubPanel>("Health"); });

    auto snap = reg.Descriptors();
    EXPECT_EQ(snap.size(), 2u);

    bool sawTransform = false;
    bool sawHealth    = false;
    for (const auto& d : snap)
    {
        if (d.componentType == std::type_index(typeid(TransformLike)))
        {
            EXPECT_EQ(d.displayName, "Transform");
            sawTransform = true;
        }
        else if (d.componentType == std::type_index(typeid(HealthLike)))
        {
            EXPECT_EQ(d.displayName, "Health");
            sawHealth = true;
        }
    }
    EXPECT_TRUE(sawTransform);
    EXPECT_TRUE(sawHealth);
}

// ─── InspectorBinder ──────────────────────────────────────────────────────────

TEST(InspectorBinderTest, NewBinderHasNoEntityAndNoSubscribers)
{
    InspectorBinder binder;
    EXPECT_FALSE(binder.HasBinding());
    EXPECT_EQ(binder.GetBoundEntity(), kInvalidInspectorEntity);
    EXPECT_EQ(binder.SubscriberCount(), 0u);
}

TEST(InspectorBinderTest, BindAndUnbindToggleSentinel)
{
    InspectorBinder binder;
    binder.BindEntity(42);
    EXPECT_TRUE(binder.HasBinding());
    EXPECT_EQ(binder.GetBoundEntity(), 42u);

    binder.UnbindAll();
    EXPECT_FALSE(binder.HasBinding());
    EXPECT_EQ(binder.GetBoundEntity(), kInvalidInspectorEntity);
}

TEST(InspectorBinderTest, EmptyCallbackRejectedWithSentinelHandle)
{
    InspectorBinder binder;
    const auto handle = binder.Subscribe(InspectorBinder::PropertyChangedFn{});
    EXPECT_EQ(handle, kInvalidSubscription);
    EXPECT_EQ(binder.SubscriberCount(), 0u);
}

TEST(InspectorBinderTest, NotifyForwardsToEverySubscriberInOrder)
{
    InspectorBinder binder;

    std::vector<std::string> seen;
    const auto h1 = binder.Subscribe(
        [&seen](const std::string& path, const std::string& value)
        {
            seen.push_back("a:" + path + "=" + value);
        });
    const auto h2 = binder.Subscribe(
        [&seen](const std::string& path, const std::string& value)
        {
            seen.push_back("b:" + path + "=" + value);
        });

    EXPECT_NE(h1, kInvalidSubscription);
    EXPECT_NE(h2, kInvalidSubscription);
    EXPECT_NE(h1, h2);
    EXPECT_EQ(binder.SubscriberCount(), 2u);

    binder.NotifyPropertyChanged("transform.position.x", "1.5");
    ASSERT_EQ(seen.size(), 2u);
    EXPECT_EQ(seen[0], "a:transform.position.x=1.5");
    EXPECT_EQ(seen[1], "b:transform.position.x=1.5");
}

TEST(InspectorBinderTest, UnsubscribeStopsFurtherDispatch)
{
    InspectorBinder binder;

    std::atomic<int> aCount{0};
    std::atomic<int> bCount{0};

    const auto handleA = binder.Subscribe(
        [&aCount](const std::string&, const std::string&) { ++aCount; });
    const auto handleB = binder.Subscribe(
        [&bCount](const std::string&, const std::string&) { ++bCount; });

    binder.NotifyPropertyChanged("path", "v");
    EXPECT_EQ(aCount.load(), 1);
    EXPECT_EQ(bCount.load(), 1);

    EXPECT_TRUE(binder.Unsubscribe(handleA));
    binder.NotifyPropertyChanged("path", "v");
    EXPECT_EQ(aCount.load(), 1);   // detached
    EXPECT_EQ(bCount.load(), 2);

    EXPECT_FALSE(binder.Unsubscribe(handleA));     // already gone
    EXPECT_FALSE(binder.Unsubscribe(kInvalidSubscription));
    EXPECT_TRUE(binder.Unsubscribe(handleB));
    EXPECT_EQ(binder.SubscriberCount(), 0u);
}

TEST(InspectorBinderTest, ThrowingSubscriberDoesNotBreakDispatch)
{
    InspectorBinder binder;
    std::atomic<int> goodCount{0};

    binder.Subscribe(
        [](const std::string&, const std::string&)
        { throw std::runtime_error("boom"); });
    binder.Subscribe(
        [&goodCount](const std::string&, const std::string&)
        { ++goodCount; });

    EXPECT_NO_THROW(binder.NotifyPropertyChanged("path", "v"));
    EXPECT_EQ(goodCount.load(), 1);
}

TEST(InspectorBinderTest, SubscriberThatUnsubscribesAnotherDuringDispatch)
{
    InspectorBinder binder;
    PropertyChangeSubscription handleB = kInvalidSubscription;
    std::atomic<int> aCount{0};
    std::atomic<int> bCount{0};

    binder.Subscribe(
        [&](const std::string&, const std::string&)
        {
            ++aCount;
            // Detach `b` while the snapshot is being walked.
            binder.Unsubscribe(handleB);
        });
    handleB = binder.Subscribe(
        [&bCount](const std::string&, const std::string&) { ++bCount; });

    binder.NotifyPropertyChanged("p", "v");
    EXPECT_EQ(aCount.load(), 1);
    EXPECT_EQ(bCount.load(), 0); // dispatch saw it had been removed mid-walk
}

TEST(InspectorBinderTest, ClearSubscribersRemovesAll)
{
    InspectorBinder binder;
    binder.Subscribe([](const std::string&, const std::string&) {});
    binder.Subscribe([](const std::string&, const std::string&) {});
    EXPECT_EQ(binder.SubscriberCount(), 2u);

    binder.ClearSubscribers();
    EXPECT_EQ(binder.SubscriberCount(), 0u);
}

} // namespace
