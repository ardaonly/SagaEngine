/// @file DiagnosticCode.h
/// @brief Stable SDE diagnostic code constants.

#pragma once

namespace SDE::DiagnosticCode
{

inline constexpr const char* UnsupportedLanguageVersion = "SDE_UNSUPPORTED_LANGUAGE_VERSION";
inline constexpr const char* UnsupportedModelVersion = "SDE_UNSUPPORTED_MODEL_VERSION";
inline constexpr const char* DuplicateModel = "SDE_DUPLICATE_MODEL";
inline constexpr const char* DuplicateInstance = "SDE_DUPLICATE_INSTANCE";
inline constexpr const char* ExpectedToken = "SDE_EXPECTED_TOKEN";

} // namespace SDE::DiagnosticCode
