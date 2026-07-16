// SPDX-License-Identifier: MPL-2.0

#include "SagaEngine/Networking/NetworkTransport.h"

#include <gtest/gtest.h>

namespace SagaEngine::Networking {
namespace {

TEST(TransportFactoryTests, ExposesOnlyTheImplementedUdpTransport)
{
    auto transport = TransportFactory::CreateUdp();

    ASSERT_NE(transport, nullptr);
    EXPECT_NE(dynamic_cast<UdpTransport*>(transport.get()), nullptr);
    EXPECT_EQ(transport->GetState(), ConnectionState::Disconnected);
}

} // namespace
} // namespace SagaEngine::Networking
