#pragma once
#include "SagaEditor/Collaboration/Sync/EditOperation.h"
namespace SagaEditor::Collaboration {
class OperationalTransform {
public:
    static EditOperation Transform(const EditOperation& op,
                                    const EditOperation& against);
};
} // namespace SagaEditor::Collaboration
