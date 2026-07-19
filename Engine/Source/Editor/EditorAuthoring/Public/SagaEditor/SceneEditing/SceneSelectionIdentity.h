// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Selection/SelectionManager.h"

#include <string_view>

namespace SagaEditor
{
[[nodiscard]] SelectableId SceneSelectionId(std::string_view sceneId,
                                            std::string_view entityId) noexcept;
}
