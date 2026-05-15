/// @file SagaSdeCompiler.h
/// @brief Product/editor bridge for real SDE project validation and compilation.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace SagaProduct
{

/// Hashes emitted by the SDE compiler for a successful project compile.
struct SagaSdeHashes
{
    std::string schemaHash;
    std::string dataHash;
    std::string dependencyHash;
    std::string compiledGraphHash;
    std::string artifactHash;
};

/// User-facing SDE compile result.
struct SagaSdeCompileResult
{
    bool                     sdeAvailable = false;
    bool                     ok = false;
    std::string              state;
    std::string              message;
    std::vector<std::string> diagnostics;
    std::optional<SagaSdeHashes> hashes;
};

/// Executes SDE through the real compiler path and preserves last good output.
class SagaSdeCompiler
{
public:
    /// Compile a project root containing schemas/ and data/.
    [[nodiscard]] SagaSdeCompileResult Compile(
        const std::filesystem::path& projectRoot);

    [[nodiscard]] const std::optional<SagaSdeHashes>& LastGoodHashes()
        const noexcept;

private:
    std::optional<SagaSdeHashes> m_lastGoodHashes;
};

} // namespace SagaProduct
