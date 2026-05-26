/// @file UiResourceIds.h
/// @brief Logical UI resource ids and validation helpers.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace SagaEngine::UI
{

/// Validation result for a logical UI resource id component.
enum class UiResourceIdValidationCode : std::uint8_t
{
    Ok = 0,
    EmptyName,
    EmptyPackageId,
    InvalidCharacter,
    InvalidPathSegment,
};

/// Detailed validation result for diagnostics and tests.
struct UiResourceIdValidationResult
{
    UiResourceIdValidationCode code = UiResourceIdValidationCode::Ok;
    std::string diagnostic;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return code == UiResourceIdValidationCode::Ok;
    }
};

/// Logical UI document id. Empty package id means loose/content UI.
struct UiDocumentId
{
    std::string packageId;
    std::string name;

    [[nodiscard]] static UiDocumentId FromContent(std::string name)
    {
        return UiDocumentId{{}, std::move(name)};
    }

    [[nodiscard]] static UiDocumentId FromPackage(
        std::string packageId,
        std::string name)
    {
        return UiDocumentId{std::move(packageId), std::move(name)};
    }

    [[nodiscard]] bool IsSet() const noexcept
    {
        return !name.empty();
    }
};

/// Logical UI style id. Empty package id means loose/content UI.
struct UiStyleId
{
    std::string packageId;
    std::string name;

    [[nodiscard]] static UiStyleId FromContent(std::string name)
    {
        return UiStyleId{{}, std::move(name)};
    }

    [[nodiscard]] static UiStyleId FromPackage(
        std::string packageId,
        std::string name)
    {
        return UiStyleId{std::move(packageId), std::move(name)};
    }

    [[nodiscard]] bool IsSet() const noexcept
    {
        return !name.empty();
    }
};

[[nodiscard]] UiResourceIdValidationResult ValidateUiResourceName(
    std::string_view name);

[[nodiscard]] UiResourceIdValidationResult ValidateUiPackageId(
    std::string_view packageId);

[[nodiscard]] UiResourceIdValidationResult ValidateUiDocumentId(
    const UiDocumentId& id);

[[nodiscard]] UiResourceIdValidationResult ValidateUiStyleId(
    const UiStyleId& id);

} // namespace SagaEngine::UI
