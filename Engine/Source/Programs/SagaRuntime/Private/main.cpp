// SPDX-License-Identifier: MPL-2.0
/// @file main.cpp
/// @brief Provides the thin SagaRuntime process entry point.

#include "RuntimeApplication.h"

int main(int argc, char* argv[])
{
    return SagaRuntimeApp::RunRuntimeApplication(argc, argv);
}
