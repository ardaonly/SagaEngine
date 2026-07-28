// SPDX-License-Identifier: MPL-2.0
#include "SagaEditor/Commands/CommandDispatcher.h"
#include "SagaEditor/Commands/CommandRegistry.h"
#include <gtest/gtest.h>

namespace SagaEditor
{
TEST(CommandDispatcherTests, NullHandlerFailsAndNotifiesPostHooks)
{
    CommandRegistry registry;
    registry.Register({"test.null", "Null", "Test", {}});
    CommandDispatcher dispatcher(registry);
    bool executed = true;
    dispatcher.AddPostHook([&](const std::string&, bool value) { executed = value; });
    EXPECT_FALSE(dispatcher.Dispatch("test.null"));
    EXPECT_FALSE(executed);
}
} // namespace SagaEditor
