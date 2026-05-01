/// @file main.cpp
/// @brief Headless smoke test for the RenderClientApp facade — placeholder.
///
/// The original implementation depended on `SagaRuntime/Client/RenderClientApp.h`,
/// a Runtime layer that does not yet exist in this repository. Until the
/// Runtime tree lands, this binary is a no-op stub so the CMake target
/// builds without dragging in a missing dependency.

#include <cstdio>

int main()
{
    std::printf("RenderClientSmokeTest: skipped (Runtime layer not yet implemented)\n");
    return 0;
}
