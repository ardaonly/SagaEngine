// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Documents/TextDocument.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>

namespace SagaEditor
{

struct SceneTransformReadModel
{
    std::array<double, 3> position{0.0, 0.0, 0.0};
    std::array<double, 4> rotation{0.0, 0.0, 0.0, 1.0};
    std::array<double, 3> scale{1.0, 1.0, 1.0};
};

struct SceneComponentReadModel
{
    std::string type;
    std::string canonicalProperties;
};

struct SceneEntityReadModel
{
    std::string entityId;
    std::optional<std::string> parentEntityId;
    std::string name;
    std::optional<SceneTransformReadModel> transform;
    std::vector<SceneComponentReadModel> components;
};

class SceneDocument final : public IDocument
{
public:
    using ChangeSubscription = std::uint64_t;
    using ChangeCallback = std::function<void()>;
    static constexpr ChangeSubscription InvalidChangeSubscription = 0;
    explicit SceneDocument(std::filesystem::path sourcePath);
    ~SceneDocument() override;

    [[nodiscard]] const DocumentSnapshot& Snapshot() const noexcept override;
    [[nodiscard]] const std::vector<DocumentDiagnostic>& Diagnostics() const noexcept override;
    [[nodiscard]] const std::string& SceneId() const noexcept;
    [[nodiscard]] const std::vector<SceneEntityReadModel>& Entities() const noexcept;
    [[nodiscard]] const SceneEntityReadModel* FindEntity(std::string_view entityId) const noexcept;

    [[nodiscard]] bool Load() override;
    [[nodiscard]] bool RenameEntity(std::string_view entityId, std::string name);
    [[nodiscard]] bool SetTransform(std::string_view entityId,
                                    const SceneTransformReadModel& transform);
    [[nodiscard]] DocumentSaveStatus Save() override;
    [[nodiscard]] bool Reload() override;
    [[nodiscard]] DocumentExternalChangeState CheckExternalChange() override;
    [[nodiscard]] DocumentCloseStatus RequestClose() const noexcept override;
    [[nodiscard]] bool CanUndo() const noexcept override;
    [[nodiscard]] bool CanRedo() const noexcept override;
    [[nodiscard]] bool Undo() override;
    [[nodiscard]] bool Redo() override;
    [[nodiscard]] ChangeSubscription SubscribeChanges(ChangeCallback callback);
    void UnsubscribeChanges(ChangeSubscription subscription);

private:
    [[nodiscard]] bool RefreshReadModel();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace SagaEditor
