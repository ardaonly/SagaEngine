/// @file DeviceDiligent.cpp
/// @brief Diligent device / context wrapper (future home of device selection,
///        adapter enumeration, and context creation).

#include "SagaEngine/RHI/IRHI.h"

namespace SagaEngine::RHI::Diligent
{

// Placeholder — device and context creation currently lives inside
// DiligentRenderBackend::Initialize().  As we refactor, adapter
// enumeration and device creation logic will be extracted here:
//
//   struct DeviceInfo { std::string adapterName; uint64_t vram; };
//   std::vector<DeviceInfo> EnumerateAdapters();
//   bool CreateDevice(const DeviceCreateDesc& desc);

} // namespace SagaEngine::RHI::Diligent
