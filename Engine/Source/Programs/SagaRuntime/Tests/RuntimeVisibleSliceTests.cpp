// SPDX-License-Identifier: MPL-2.0

#include "RuntimeStateStore.h"
#include "RuntimeUiPromptController.h"

#include <gtest/gtest.h>

namespace
{
namespace Play = SagaRuntime::Play;
namespace Scripting = SagaEngine::Scripting;
using SagaRuntimeApp::RuntimeStateStore;
using SagaRuntimeApp::RuntimeUiPromptController;

Scripting::ScriptStateContext Context(std::uint64_t token = 9)
{
    return {{4}, {7}, "StarterArena.DoorController",
            "StarterArena.DoorController", token};
}

Play::RuntimeUiPromptBinding Prompt()
{
    return {"Interact", "StarterArena.DoorController", "interaction-prompt",
            "door-root", "Open Door", "door.target-open", "Close Door", "Always"};
}

Play::RuntimeInputMapArtifact Input(std::string label = "E")
{
    Play::RuntimeInputMapArtifact input;
    input.actions.push_back({"Interact", "Interact", "Gameplay",
        {{"Keyboard", label, label, "Keyboard " + label}}});
    return input;
}

RuntimeStateStore Store(std::string key = "door.target-open")
{
    return RuntimeStateStore({{"StarterArena.DoorController", {std::move(key)}}});
}

TEST(RuntimeStateStoreTests, PublishesMonotonicContextualSnapshots)
{
    auto store = Store();
    auto context = Context();
    store.OnInstanceActivated(context);
    EXPECT_EQ(store.KeyCount(), 0U);
    ASSERT_TRUE(store.SetBool(context, "door.target-open", false).Succeeded());
    EXPECT_EQ(store.KeyCount(), 1U);
    const auto closed = store.Snapshot(context.entity, context.behaviorId, "door.target-open");
    ASSERT_TRUE(closed.has_value());
    EXPECT_FALSE(closed->value);
    ASSERT_TRUE(store.SetBool(context, "door.target-open", true).Succeeded());
    EXPECT_EQ(store.KeyCount(), 1U);
    const auto opened = store.Snapshot(context.entity, context.behaviorId, "door.target-open");
    ASSERT_TRUE(opened.has_value());
    EXPECT_TRUE(opened->value);
    EXPECT_GT(opened->revision, closed->revision);
    EXPECT_EQ(opened->activeInstanceToken, context.activeInstanceToken);
}

TEST(RuntimeStateStoreTests, RejectsStaleAndDestroyedInstances)
{
    auto store = Store("light.enabled");
    auto current = Context();
    store.OnInstanceActivated(current);
    auto stale = Context(8);
    EXPECT_FALSE(store.SetBool(stale, "light.enabled", true).Succeeded());
    EXPECT_FALSE(store.SetBool(current, "untrusted.key", true).Succeeded());
    ASSERT_TRUE(store.SetBool(current, "light.enabled", true).Succeeded());
    store.OnInstanceDeactivated(stale);
    EXPECT_TRUE(store.Snapshot(current.entity, current.behaviorId,
                              "light.enabled").has_value());
    store.OnInstanceDeactivated(current);
    EXPECT_EQ(store.KeyCount(), 0U);
    EXPECT_FALSE(store.Snapshot(current.entity, current.behaviorId,
                               "light.enabled").has_value());
}

TEST(RuntimeUiPromptControllerTests, JoinsVerifiedDisplayLabelAndState)
{
    auto store = Store();
    auto context = Context();
    store.OnInstanceActivated(context);
    ASSERT_TRUE(store.SetBool(context, "door.target-open", false).Succeeded());
    EXPECT_EQ(RuntimeUiPromptController::Compose(
        Prompt(), Input("E"), store, context.entity), "[E] Open Door");
    ASSERT_TRUE(store.SetBool(context, "door.target-open", true).Succeeded());
    EXPECT_EQ(RuntimeUiPromptController::Compose(
        Prompt(), Input("F"), store, context.entity), "[F] Close Door");
}

TEST(RuntimeUiPromptControllerTests, FailsClosedForMissingActionOrState)
{
    auto store = Store();
    auto context = Context();
    store.OnInstanceActivated(context);
    auto input = Input();
    input.actions.front().actionId = "Jump";
    EXPECT_FALSE(RuntimeUiPromptController::Compose(
        Prompt(), input, store, context.entity).has_value());
    EXPECT_FALSE(RuntimeUiPromptController::Compose(
        Prompt(), Input(), store, context.entity).has_value());
}

} // namespace
