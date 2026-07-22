// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/VisualBlocks/ReadOnlyScriptProjection.h"

#include <utility>

namespace SagaEditor::VisualBlocks
{
ProjectionCard::ProjectionCard(std::string id, std::string kind,
    std::string displayName, std::string packageOrigin,
    ProjectionSourceSpan source, std::vector<ProjectionCard> children)
    : id_(std::move(id)), kind_(std::move(kind)),
      displayName_(std::move(displayName)), packageOrigin_(std::move(packageOrigin)),
      source_(std::move(source)), children_(std::move(children)) {}

ReadOnlyScriptProjection::ReadOnlyScriptProjection(std::string projectId,
    std::string projectHash, std::string sourceRevision,
    std::string packageGenerationId, std::string packageGenerationHash,
    std::string artifactHash, std::vector<ProjectionCard> roots,
    std::vector<ProjectionCard> opaqueRegions)
    : projectId_(std::move(projectId)), projectHash_(std::move(projectHash)),
      sourceRevision_(std::move(sourceRevision)),
      packageGenerationId_(std::move(packageGenerationId)),
      packageGenerationHash_(std::move(packageGenerationHash)),
      artifactHash_(std::move(artifactHash)), roots_(std::move(roots)),
      opaqueRegions_(std::move(opaqueRegions)) {}
}
