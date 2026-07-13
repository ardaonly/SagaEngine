/// @file RuntimeApplication.h
/// @brief Declares the standalone SagaRuntime application runner.

#pragma once

namespace SagaRuntimeApp
{

/// Parse runtime arguments and delegate to a supported bounded host mode.
[[nodiscard]] int RunRuntimeApplication(int argc, char* argv[]);

} // namespace SagaRuntimeApp
