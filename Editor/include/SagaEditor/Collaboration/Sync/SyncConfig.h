#pragma once
#include <cstdint>
namespace SagaEditor::Collaboration {
struct SyncConfig {
    uint32_t flushIntervalMs = 50;
    uint32_t maxBatchSize    = 32;
    bool     enableCompression = false;
};
} // namespace SagaEditor::Collaboration
