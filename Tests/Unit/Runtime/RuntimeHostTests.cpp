/// @file RuntimeHostTests.cpp
/// @brief Verifies the app-private standalone runtime host boundary.

#include "RuntimeHost.h"

#include <gtest/gtest.h>

namespace
{

TEST(RuntimeHostTests, LegacyConnectedClientModeFailsWithStableDiagnostic)
{
    const SagaRuntimeApp::RuntimeHostResult result =
        SagaRuntimeApp::RuntimeHost{}.Run();

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.exitCode, 2);
    EXPECT_EQ(
        result.diagnosticId,
        SagaRuntimeApp::RuntimeHostDiagnostics::LegacyConnectedClientUnsupported);
    EXPECT_NE(result.message.find("SagaClient"), std::string::npos);
    EXPECT_NE(result.message.find("legacy connected-client mode"),
              std::string::npos);
}

} // namespace
