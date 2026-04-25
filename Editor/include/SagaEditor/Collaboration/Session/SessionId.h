#pragma once
#include <string>
namespace SagaEditor::Collaboration {
using SessionId = std::string;
SessionId GenerateSessionId();
} // namespace SagaEditor::Collaboration
