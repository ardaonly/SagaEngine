#pragma once
#include "ReplicationManager.h"

#define SAGA_RPC_REGISTER(manager, name, func, requiresAuth) \
    (manager).RegisterRPC(name, func, requiresAuth)

#define SAGA_RPC_CALL(manager, rpcId, ...) \
    do { \
        Packet pkt(PacketType::RPCRequest); \
        pkt.Write(static_cast<uint32_t>(rpcId)); \
        /* Serialize args and send via NetworkTransport */ \
    } while(0)