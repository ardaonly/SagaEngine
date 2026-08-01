// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "SagaEditor/Documents/SourceDocuments.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEditor
{
class SagaScriptLanguageServiceSession;
class UndoRedoStack;

enum class ControlledBlockEditState
{
    Ready,
    Planning,
    AwaitingReview,
    Applying,
    Analyzing,
    Refreshing,
    StaleDocument,
    GenerationChanged,
    SpanConflict,
    SemanticRejected,
    OpaqueReadOnly,
    ServiceRecovering,
    Conflict,
};

struct ControlledBlockEditPlan final
{
    std::string operationId;
    std::string projectSessionId;
    std::string projectSnapshotIdentity;
    std::string packageGeneration;
    std::string documentId;
    std::uint64_t documentVersion = 0;
    std::string documentHash;
    std::string fieldId;
    std::uint64_t startByte = 0;
    std::uint64_t endByte = 0;
    std::string oldChecksum;
    std::string oldValue;
    std::string replacement;
    std::string candidateText;
    std::string candidateHash;
    std::string minimalDiff;
    bool parsePassed = false;
    bool semanticPassed = false;
    bool analyzersPassed = false;
    bool packageVisibilityPassed = false;
    bool accepted = false;
    std::vector<std::string> diagnostics;
};

class ControlledBlockEditController final
{
public:
    ControlledBlockEditController(SagaScriptLanguageServiceSession& languageService,
        UndoRedoStack& journal);
    [[nodiscard]] ControlledBlockEditPlan Plan(CSharpSourceDocument& document,
        std::string fieldId, std::string value);
    [[nodiscard]] bool Apply(CSharpSourceDocument& document,
        const ControlledBlockEditPlan& plan);
    void Cancel();
    [[nodiscard]] ControlledBlockEditState State() const noexcept { return state_; }
    [[nodiscard]] const std::string& Diagnostic() const noexcept { return diagnostic_; }

private:
    SagaScriptLanguageServiceSession& languageService_;
    UndoRedoStack& journal_;
    ControlledBlockEditState state_ = ControlledBlockEditState::Ready;
    std::string diagnostic_;
    std::uint64_t nextOperation_ = 1;
};
}
