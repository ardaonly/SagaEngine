/// @file BlockToIRLowerer.h
/// @brief Walks a `BlockScript` and emits a canonical `IRProgram`.

#pragma once

#include "SagaEditor/VisualScripting/Blocks/BlockIR.h"

#include <string>
#include <vector>

namespace SagaEditor::VisualScripting
{

class BlockLibrary;
class BlockScript;

// ─── Lower Diagnostic ─────────────────────────────────────────────────────────

/// One issue raised by the lowerer. The editor surfaces these as
/// inline warnings on the canvas. A program with `Severity::Error`
/// issues should not be passed to the runtime compiler.
struct LowerDiagnostic
{
    enum class Severity : std::uint8_t { Warning, Error };

    Severity    severity = Severity::Warning;
    std::string message;
    std::string opcode;       ///< Block whose lowering produced the issue (may be empty).
};

// ─── Lower Result ─────────────────────────────────────────────────────────────

struct LowerResult
{
    IRProgram                    program;
    std::vector<LowerDiagnostic> diagnostics;

    /// True when no `Error`-severity diagnostics were emitted. Warning-only
    /// programs are still safe to compile.
    [[nodiscard]] bool Ok() const noexcept;
};

// ─── Lowerer ──────────────────────────────────────────────────────────────────

/// Pure function — no editor state, no script mutation. Builds one
/// entry-point procedure per Hat block; the procedure body is the
/// stack rooted at that hat. Reporter expressions inside slot fills
/// recursively lower to `IRValue::Call`. Unknown opcodes produce
/// `Error` diagnostics; the resulting IR is still well-formed but
/// callers should refuse to run it.
[[nodiscard]] LowerResult LowerBlockScript(const BlockScript&  script,
                                            const BlockLibrary& library);

} // namespace SagaEditor::VisualScripting
