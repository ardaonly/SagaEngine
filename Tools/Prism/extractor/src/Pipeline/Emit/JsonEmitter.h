/// @file JsonEmitter.h
/// @brief Serialize an ExtractionResult to a self-contained raw JSON file.
///        The output is the stable contract consumed by the Python pipeline.

#pragma once

#include "Record.h"

#include <filesystem>
#include <string>

namespace prism {

/// Write *result* as indented UTF-8 JSON to *out_path*.
/// Creates parent directories when they do not exist.
/// Throws std::runtime_error on I/O failure.
void EmitJson(const ExtractionResult&      result,
              const std::string&           repo_root,
              const std::filesystem::path& out_path);

} // namespace prism
