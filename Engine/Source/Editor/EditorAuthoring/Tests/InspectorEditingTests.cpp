/// @file InspectorEditingTests.cpp
/// @brief GoogleTest coverage for the InspectorEditing primitives.
///
/// Targets `ComponentEditorRegistry`, `InspectorBinder`, and
/// `PropertyEditorFactory`. These three types are the framework-free
/// core of the inspector — they have no Qt dependency, so unit
/// coverage runs in the engine's existing GoogleTest harness without
/// linking the editor's UI backend.

#include "SagaEditor/InspectorEditing/ComponentEditorRegistry.h"
#include "SagaEditor/InspectorEditing/InspectorBinder.h"
#include "SagaEditor/InspectorEditing/IPropertyEditor.h"
#include "SagaEditor/InspectorEditing/IPropertyEditorBackend.h"
#include "SagaEditor/InspectorEditing/PropertyEditorFactory.h"
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
    [[nodiscard]] void*       GetNativeWidget() const noexcept override { return nullptr; }

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

// ─── PropertyEditorFactory ────────────────────────────────────────────────────

TEST(PropertyEditorFactoryTest, MakeFillsDescriptor)
{
    const auto desc =
        PropertyDescriptor::Make(PropertyType::Vec3, "Position", "transform.position");
    EXPECT_EQ(desc.type,        PropertyType::Vec3);
    EXPECT_EQ(desc.label,       "Position");
    EXPECT_EQ(desc.bindingPath, "transform.position");
    EXPECT_TRUE(desc.toolTip.empty());
    EXPECT_TRUE(desc.unitSuffix.empty());
    EXPECT_FALSE(desc.readOnly);
}

TEST(PropertyEditorFactoryTest, CreateReturnsNullWhenNoBackendInstalled)
{
    // The factory dispatches through whatever backend is installed at
    // process scope. The unit-test harness installs no backend, so
    // every primitive type must yield nullptr here and Supports must
    // report false. The test detaches the backend defensively so a
    // sibling test that installs one cannot leak state into this one.
    PropertyEditorFactory::SetBackend(nullptr);

    const PropertyType all[] = {
        PropertyType::Bool,  PropertyType::Int,    PropertyType::Float,
        PropertyType::String, PropertyType::Vec2,  PropertyType::Vec3,
        PropertyType::Vec4,   PropertyType::Quat,  PropertyType::Color,
        PropertyType::Enum,   PropertyType::Asset,
    };
    for (auto t : all)
    {
        EXPECT_EQ(PropertyEditorFactory::Create(t, "Label", "binding.path"), nullptr);
        EXPECT_FALSE(PropertyEditorFactory::Supports(t));
    }
    EXPECT_EQ(PropertyEditorFactory::GetBackend(), nullptr);
}

// ─── Backend Injection ────────────────────────────────────────────────────────

/// Minimal stub editor used to exercise backend dispatch without
/// pulling in any UI framework headers.
class StubPropertyEditor final : public IPropertyEditor
{
public:
    explicit StubPropertyEditor(PropertyDescriptor desc)
        : m_descriptor(std::move(desc))
    {}

    [[nodiscard]] PanelId     GetPanelId()      const override
    {
        return "saga.test.property";
    }
    [[nodiscard]] std::string GetTitle()        const override
    {
        return m_descriptor.label;
    }
    [[nodiscard]] void*       GetNativeWidget() const noexcept override
    {
        return nullptr;
    }

    [[nodiscard]] PropertyType GetPropertyType() const noexcept override
    {
        return m_descriptor.type;
    }
    [[nodiscard]] const std::string& GetLabel() const noexcept override
    {
        return m_descriptor.label;
    }
    [[nodiscard]] const std::string& GetBindingPath() const noexcept override
    {
        return m_descriptor.bindingPath;
    }

    [[nodiscard]] std::string GetValueAsString() const override
    {
        return m_value;
    }
    bool SetValueFromString(std::string_view value) override
    {
        m_value.assign(value);
        return true;
    }
    void SetOnValueChanged(ValueChangedFn callback) override
    {
        m_onChange = std::move(callback);
    }
    void SetReadOnly(bool readOnly) noexcept override { m_readOnly = readOnly; }
    [[nodiscard]] bool IsReadOnly() const noexcept override { return m_readOnly; }

    /// Test hook that simulates a user committing a new value.
    void CommitValue(std::string value)
    {
        m_value = std::move(value);
        if (m_onChange) m_onChange(m_value);
    }

private:
    PropertyDescriptor m_descriptor;
    std::string        m_value;
    ValueChangedFn     m_onChange;
    bool               m_readOnly = false;
};

class StubPropertyEditorBackend final : public IPropertyEditorBackend
{
public:
    [[nodiscard]] std::unique_ptr<IPropertyEditor>
        Create(const PropertyDescriptor& descriptor) override
    {
        ++createCount;
        return std::make_unique<StubPropertyEditor>(descriptor);
    }

    [[nodiscard]] bool Supports(PropertyType type) const noexcept override
    {
        // Asset is intentionally unsupported to exercise the fallback.
        return type != PropertyType::Asset;
    }

    int createCount = 0;
};

class PropertyEditorFactoryBackendTest : public ::testing::Test
{
protected:
    void SetUp() override    { PropertyEditorFactory::SetBackend(&m_backend); }
    void TearDown() override { PropertyEditorFactory::SetBackend(nullptr); }

    StubPropertyEditorBackend m_backend;
};

TEST_F(PropertyEditorFactoryBackendTest, CreateGoesThroughInstalledBackend)
{
    auto editor = PropertyEditorFactory::Create(PropertyType::Float,
                                                "Mass",
                                                "rigidbody.mass");
    ASSERT_NE(editor.get(), nullptr);
    EXPECT_EQ(m_backend.createCount, 1);
    EXPECT_EQ(editor->GetTitle(), "Mass");

    auto* typed = dynamic_cast<IPropertyEditor*>(editor.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->GetPropertyType(),  PropertyType::Float);
    EXPECT_EQ(typed->GetLabel(),         "Mass");
    EXPECT_EQ(typed->GetBindingPath(),   "rigidbody.mass");
    EXPECT_FALSE(typed->IsReadOnly());
}

TEST_F(PropertyEditorFactoryBackendTest, SupportsReflectsBackendCapabilities)
{
    EXPECT_TRUE(PropertyEditorFactory::Supports(PropertyType::Float));
    EXPECT_TRUE(PropertyEditorFactory::Supports(PropertyType::Vec3));
    EXPECT_FALSE(PropertyEditorFactory::Supports(PropertyType::Asset));
}

TEST_F(PropertyEditorFactoryBackendTest, EditorRoundTripsValueAndFiresCallback)
{
    auto editor = PropertyEditorFactory::Create(PropertyType::String,
                                                "Name",
                                                "tag.name");
    ASSERT_NE(editor.get(), nullptr);
    auto* stub = dynamic_cast<StubPropertyEditor*>(editor.get());
    ASSERT_NE(stub, nullptr);

    EXPECT_TRUE(stub->SetValueFromString("Initial"));
    EXPECT_EQ(stub->GetValueAsString(), "Initial");

    std::string lastFired;
    stub->SetOnValueChanged(
        [&lastFired](const std::string& v) { lastFired = v; });

    stub->CommitValue("Committed");
    EXPECT_EQ(stub->GetValueAsString(), "Committed");
    EXPECT_EQ(lastFired,                "Committed");
}

TEST(PropertyEditorFactoryTest, TypeIdsAreStableAndUnique)
{
    const PropertyType all[] = {
        PropertyType::Bool,  PropertyType::Int,    PropertyType::Float,
        PropertyType::String, PropertyType::Vec2,  PropertyType::Vec3,
        PropertyType::Vec4,   PropertyType::Quat,  PropertyType::Color,
        PropertyType::Enum,   PropertyType::Asset,
    };
    std::vector<std::string_view> seen;
    for (auto t : all)
    {
        const auto id = PropertyEditorFactory::TypeId(t);
        EXPECT_FALSE(id.empty());
        for (const auto& prev : seen)
        {
            EXPECT_NE(prev, id);
        }
        seen.push_back(id);
    }

    EXPECT_EQ(PropertyEditorFactory::TypeId(PropertyType::Vec3),
              std::string_view{"saga.prop.vec3"});
    EXPECT_EQ(PropertyEditorFactory::TypeId(PropertyType::Asset),
              std::string_view{"saga.prop.asset"});
}

} // namespace
