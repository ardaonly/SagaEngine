// SPDX-License-Identifier: MPL-2.0

#include "SagaEngine/ServerAuthority/ZoneServer.h"

#include <gtest/gtest.h>

namespace SagaEngine::ServerAuthority {
namespace {

TEST(ZoneServerSecurityBoundaryTests, DefaultConfigurationIsLoopbackOnly)
{
    const ZoneServerConfig config;
    EXPECT_EQ(config.bindAddress, "127.0.0.1");
}

TEST(ZoneServerSecurityBoundaryTests, PublicBindFailsBeforeStartingSubsystems)
{
    ZoneServerConfig config;
    config.bindAddress = "0.0.0.0";
    config.port = 0;
    ZoneServer server(config);

    EXPECT_FALSE(server.Initialize());
    EXPECT_EQ(server.GetBoundPort(), 0u);
}

TEST(ZoneServerSecurityBoundaryTests, InvalidBindAddressFailsClosed)
{
    ZoneServerConfig config;
    config.bindAddress = "not-an-address";
    config.port = 0;
    ZoneServer server(config);

    EXPECT_FALSE(server.Initialize());
    EXPECT_EQ(server.GetBoundPort(), 0u);
}

} // namespace
} // namespace SagaEngine::ServerAuthority
