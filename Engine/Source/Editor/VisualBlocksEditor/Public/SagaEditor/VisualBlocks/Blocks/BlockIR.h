/// @file BlockIR.h
/// @brief Canonical IR shared by block authoring and the future C# parser.
///
/// The block lowerer (`BlockToIRLowerer`) and the future C# subset
/// parser both emit `IRProgram`. The runtime compiler (in
/// SHARED_ROADMAP §2) consumes `IRProgram` regardless of which
/// front-end produced it. Keeping the IR design here — inside
/// `VisualScripting/Blocks/` — is deliberate: blocks are the first
/// front-end, the IR's shape is settled by what blocks need, and
/// the C# parser will follow.
///
/// Design notes:
///   - `IRProgram` is a list of procedures. Each Hat block produces
///     one entry-point procedure; each "My Blocks" custom block
///     produces one user procedure.
///   - `IRProcedure` is a list of statements in execution order.
///     Branches and loops carry nested statement lists rather than
///     basic-block labels — the IR is hierarchical, not flat.
///   - `IRValue` is a typed expression tree. Reporter blocks lower
///     to `IRValue::Call`; literals lower to `IRValue::Literal`;
///     variable getters lower to `IRValue::VariableRef`.
///   - The IR is value-semantic and POD-like so tests can compare
///     full programs by `operator==`.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace SagaEditor::VisualScripting
{

// ─── IR Type ──────────────────────────────────────────────────────────────────

/// Type vocabulary. `Any` is used by polymorphic variable getters; the
/// runtime compiler resolves it from the variable's declared type.
enum class IRType : std::uint8_t
{
    Void,
    Number,
    Text,
    Boolean,
    Color,
    Any,
};

[[nodiscard]] const char* IRTypeId(IRType type) noexcept;
[[nodiscard]] IRType      IRTypeFromId(const std::string& id) noexcept;

// ─── IR Value ─────────────────────────────────────────────────────────────────

struct IRValue;
using IRValuePtr = std::shared_ptr<IRValue>;

/// Expression tree node. The runtime compiler walks this recursively;
/// the editor surfaces it for the inspector's "watch" panel and for
/// the IR diff tool that compares two graphs to detect drift.
struct IRValue
{
    enum class Tag : std::uint8_t
    {
        Empty,        ///< Slot was empty; consumer falls back to the slot's literal default.
        Literal,      ///< Inline literal (Number / Text / Boolean / Color).
        VariableRef,  ///< Reference to a declared variable by id.
        Call,         ///< Reporter / Boolean call: opcode + arguments.
    };

    Tag         tag  = Tag::Empty;
    IRType      type = IRType::Void;

    // Literal payload — only the field matching `type` is meaningful.
    double        literalNumber = 0.0;
    std::string   literalText;
    bool          literalBool   = false;
    std::uint32_t literalColor  = 0xFFFFFFFFu;

    // VariableRef payload.
    std::string   variableId;

    // Call payload.
    std::string                 opcode;
    std::vector<IRValuePtr>     arguments;

    // ─── Builders ─────────────────────────────────────────────────────────────

    [[nodiscard]] static IRValuePtr MakeEmpty();
    [[nodiscard]] static IRValuePtr MakeNumber(double v);
    [[nodiscard]] static IRValuePtr MakeText(std::string v);
    [[nodiscard]] static IRValuePtr MakeBool(bool v);
    [[nodiscard]] static IRValuePtr MakeColor(std::uint32_t rgba);
    [[nodiscard]] static IRValuePtr MakeVariableRef(std::string variableId,
                                                     IRType      varType = IRType::Any);
    [[nodiscard]] static IRValuePtr MakeCall(std::string opcode,
                                              IRType      returnType,
                                              std::vector<IRValuePtr> args);

    // Equality (deep — walks the argument tree).
    [[nodiscard]] bool Equals(const IRValue& other) const noexcept;
};

// ─── IR Statement ─────────────────────────────────────────────────────────────

struct IRStatement;
using IRStatementPtr = std::shared_ptr<IRStatement>;

/// Single executable step inside a procedure body.
struct IRStatement
{
    enum class Tag : std::uint8_t
    {
        Call,        ///< Stack block call: opcode + argument expressions.
        If,          ///< Single-arm if (no else).
        IfElse,      ///< Two-arm if / else.
        Repeat,      ///< Repeat N times (count is an `IRValue`).
        Forever,     ///< Loop until Stop / Cap.
        Stop,        ///< Cap block — terminates the enclosing procedure.
        ProcedureCall, ///< Call into a user-defined custom block ("My Blocks").
    };

    Tag         tag = Tag::Call;

    // Call payload.
    std::string                 opcode;
    std::vector<IRValuePtr>     arguments;

    // Branch payloads.
    IRValuePtr                  condition;       ///< If / IfElse — boolean expression.
    std::vector<IRStatementPtr> thenBody;        ///< If / IfElse / Repeat / Forever body.
    std::vector<IRStatementPtr> elseBody;        ///< IfElse only.
    IRValuePtr                  repeatCount;     ///< Repeat — number expression.

    // ProcedureCall payload.
    std::string                 procedureId;     ///< User-defined procedure id.

    // ─── Builders ─────────────────────────────────────────────────────────────

    [[nodiscard]] static IRStatementPtr MakeCall(std::string             opcode,
                                                  std::vector<IRValuePtr> args);
    [[nodiscard]] static IRStatementPtr MakeIf  (IRValuePtr                  condition,
                                                  std::vector<IRStatementPtr> body);
    [[nodiscard]] static IRStatementPtr MakeIfElse(IRValuePtr                  condition,
                                                    std::vector<IRStatementPtr> thenBody,
                                                    std::vector<IRStatementPtr> elseBody);
    [[nodiscard]] static IRStatementPtr MakeRepeat(IRValuePtr                  count,
                                                    std::vector<IRStatementPtr> body);
    [[nodiscard]] static IRStatementPtr MakeForever(std::vector<IRStatementPtr> body);
    [[nodiscard]] static IRStatementPtr MakeStop();
    [[nodiscard]] static IRStatementPtr MakeProcedureCall(std::string             id,
                                                           std::vector<IRValuePtr> args);
};

// ─── IR Procedure ─────────────────────────────────────────────────────────────

/// Single procedure — either an event-handler (`isEntryPoint = true`)
/// produced by a Hat block, or a user-defined custom block.
struct IRProcedureParameter
{
    std::string name;
    IRType      type = IRType::Any;
};

struct IRProcedure
{
    std::string                       id;            ///< Unique within the program.
    std::string                       displayName;   ///< User-visible label.
    bool                              isEntryPoint   = false;
    std::string                       eventId;       ///< For entry points only.
    std::vector<IRProcedureParameter> parameters;    ///< Empty for entry points.
    std::vector<IRStatementPtr>       body;
};

// ─── IR Program ───────────────────────────────────────────────────────────────

/// Top-level IR container. Carries every procedure plus declared
/// variables. The runtime compiler reads the program once and emits
/// the compiled output.
struct IRVariableDecl
{
    std::string id;
    std::string displayName;
    IRType      type = IRType::Any;
};

struct IRProgram
{
    std::vector<IRProcedure>     procedures;
    std::vector<IRVariableDecl>  variables;

    /// Find a procedure by id; returns nullptr when not found.
    [[nodiscard]] const IRProcedure* FindProcedure(const std::string& id) const noexcept;

    /// Collect every entry-point procedure (Hat blocks).
    [[nodiscard]] std::vector<const IRProcedure*> EntryPoints() const;
};

} // namespace SagaEditor::VisualScripting
