// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace SagaEditor::VisualBlocks
{
struct ProjectionSourceSpan final
{
    std::string sourcePath;
    std::string sourceHash;
    std::uint64_t startByte = 0;
    std::uint64_t endByte = 0;
    std::uint32_t startLine = 0;
    std::uint32_t startColumn = 0;
    std::uint32_t endLine = 0;
    std::uint32_t endColumn = 0;

    bool operator==(const ProjectionSourceSpan&) const = default;
};

struct EditableProjectionField final
{
    std::string fieldId;
    std::string valueType;
    std::string minimum;
    std::string containingSymbol;
    std::string initializerRole;
    ProjectionSourceSpan source;
    std::string sourceRevision;
    std::string packageGenerationId;
    std::string packageGenerationHash;
    std::string checksum;
    std::string value;

    bool operator==(const EditableProjectionField&) const = default;
};

class ProjectionCard final
{
public:
    ProjectionCard(std::string id, std::string kind, std::string displayName,
        std::string packageOrigin, ProjectionSourceSpan source,
        std::vector<ProjectionCard> children = {});

    [[nodiscard]] const std::string& Id() const noexcept { return id_; }
    [[nodiscard]] const std::string& Kind() const noexcept { return kind_; }
    [[nodiscard]] const std::string& DisplayName() const noexcept { return displayName_; }
    [[nodiscard]] const std::string& PackageOrigin() const noexcept { return packageOrigin_; }
    [[nodiscard]] const ProjectionSourceSpan& Source() const noexcept { return source_; }
    [[nodiscard]] const std::vector<ProjectionCard>& Children() const noexcept { return children_; }

private:
    std::string id_;
    std::string kind_;
    std::string displayName_;
    std::string packageOrigin_;
    ProjectionSourceSpan source_;
    std::vector<ProjectionCard> children_;
};

class ReadOnlyScriptProjection final
{
public:
    ReadOnlyScriptProjection(std::string projectId, std::string projectHash,
        std::string sourceRevision, std::string packageGenerationId,
        std::string packageGenerationHash, std::string artifactHash,
        std::vector<ProjectionCard> roots, std::vector<ProjectionCard> opaqueRegions,
        std::vector<EditableProjectionField> editableFields = {});

    [[nodiscard]] const std::string& ProjectId() const noexcept { return projectId_; }
    [[nodiscard]] const std::string& ProjectHash() const noexcept { return projectHash_; }
    [[nodiscard]] const std::string& SourceRevision() const noexcept { return sourceRevision_; }
    [[nodiscard]] const std::string& PackageGenerationId() const noexcept { return packageGenerationId_; }
    [[nodiscard]] const std::string& PackageGenerationHash() const noexcept { return packageGenerationHash_; }
    [[nodiscard]] const std::string& ArtifactHash() const noexcept { return artifactHash_; }
    [[nodiscard]] const std::vector<ProjectionCard>& Roots() const noexcept { return roots_; }
    [[nodiscard]] const std::vector<ProjectionCard>& OpaqueRegions() const noexcept { return opaqueRegions_; }
    [[nodiscard]] const std::vector<EditableProjectionField>& EditableFields() const noexcept
    { return editableFields_; }

private:
    std::string projectId_;
    std::string projectHash_;
    std::string sourceRevision_;
    std::string packageGenerationId_;
    std::string packageGenerationHash_;
    std::string artifactHash_;
    std::vector<ProjectionCard> roots_;
    std::vector<ProjectionCard> opaqueRegions_;
    std::vector<EditableProjectionField> editableFields_;
};
}
