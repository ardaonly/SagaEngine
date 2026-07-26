// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "SagaEditor/Project/EditorProjectWorkflowController.h"

namespace SagaEditor
{

/// Builds Qt dialog callbacks. Dialogs resolve the active editor window when run.
[[nodiscard]] ProjectWorkflowPrompts MakeQtProjectWorkflowPrompts();

} // namespace SagaEditor
