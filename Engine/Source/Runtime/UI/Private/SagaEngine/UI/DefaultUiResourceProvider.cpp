/// @file DefaultUiResourceProvider.cpp
/// @brief Filesystem-backed logical UI resource provider.

#include "SagaEngine/UI/IUiResourceProvider.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace SagaEngine::UI
{
namespace
{

[[nodiscard]] bool IsTokenCharacter(char value) noexcept
{
    const unsigned char ch = static_cast<unsigned char>(value);
    return std::isalnum(ch) != 0 || value == '_' || value == '-';
}

[[nodiscard]] bool IsNameCharacter(char value) noexcept
{
    return IsTokenCharacter(value) || value == '/' || value == '.';
}

[[nodiscard]] bool ContainsInvalidSegment(std::string_view value) noexcept
{
    std::size_t segmentStart = 0;
    while (segmentStart <= value.size())
    {
        const std::size_t segmentEnd = value.find('/', segmentStart);
        const std::string_view segment =
            value.substr(segmentStart, segmentEnd - segmentStart);
        if (segment.empty() || segment == "." || segment == "..")
        {
            return true;
        }

        if (segmentEnd == std::string_view::npos)
        {
            break;
        }
        segmentStart = segmentEnd + 1;
    }

    return false;
}

[[nodiscard]] std::filesystem::path WithExtension(
    const std::string& name,
    const char* extension)
{
    std::filesystem::path path(name);
    if (path.extension().empty())
    {
        path += extension;
    }
    return path;
}

[[nodiscard]] std::filesystem::path UiPath(
    const std::filesystem::path& root,
    const std::string& name,
    const char* extension)
{
    return root / "UI" / WithExtension(name, extension);
}

[[nodiscard]] UiResourceResolveResult InvalidIdResult(std::string diagnostic)
{
    return UiResourceResolveResult{
        UiResourceResolveStatus::InvalidId,
        {},
        std::move(diagnostic),
    };
}

} // namespace

UiResourceIdValidationResult ValidateUiResourceName(std::string_view name)
{
    if (name.empty())
    {
        return {
            UiResourceIdValidationCode::EmptyName,
            "UI resource id name is empty.",
        };
    }

    for (const char ch : name)
    {
        if (!IsNameCharacter(ch))
        {
            return {
                UiResourceIdValidationCode::InvalidCharacter,
                "UI resource id name contains an invalid character.",
            };
        }
    }

    if (name.front() == '/' || name.back() == '/' ||
        ContainsInvalidSegment(name))
    {
        return {
            UiResourceIdValidationCode::InvalidPathSegment,
            "UI resource id name contains an invalid path segment.",
        };
    }

    return {};
}

UiResourceIdValidationResult ValidateUiPackageId(std::string_view packageId)
{
    if (packageId.empty())
    {
        return {
            UiResourceIdValidationCode::EmptyPackageId,
            "UI package id is empty.",
        };
    }

    for (const char ch : packageId)
    {
        if (!IsTokenCharacter(ch))
        {
            return {
                UiResourceIdValidationCode::InvalidCharacter,
                "UI package id contains an invalid character.",
            };
        }
    }

    return {};
}

UiResourceIdValidationResult ValidateUiDocumentId(const UiDocumentId& id)
{
    if (!id.packageId.empty())
    {
        const UiResourceIdValidationResult packageResult =
            ValidateUiPackageId(id.packageId);
        if (!packageResult.Succeeded())
        {
            return packageResult;
        }
    }

    return ValidateUiResourceName(id.name);
}

UiResourceIdValidationResult ValidateUiStyleId(const UiStyleId& id)
{
    if (!id.packageId.empty())
    {
        const UiResourceIdValidationResult packageResult =
            ValidateUiPackageId(id.packageId);
        if (!packageResult.Succeeded())
        {
            return packageResult;
        }
    }

    return ValidateUiResourceName(id.name);
}

namespace
{

class DefaultUiResourceProvider final : public IUiResourceProvider
{
public:
    [[nodiscard]] UiResourceMountResult MountContentRoot(
        std::filesystem::path root) override
    {
        if (root.empty())
        {
            return {
                UiResourceMountStatus::InvalidRoot,
                "UI content mount root is empty.",
            };
        }

        root = std::filesystem::absolute(std::move(root)).lexically_normal();
        if (ContainsContentRoot(root))
        {
            return {
                UiResourceMountStatus::DuplicateMount,
                "UI content mount root is already registered.",
            };
        }

        contentRoots_.push_back(std::move(root));
        return {UiResourceMountStatus::Ok, {}};
    }

    [[nodiscard]] UiResourceMountResult MountPackageRoot(
        std::string packageId,
        std::filesystem::path root) override
    {
        const UiResourceIdValidationResult idResult =
            ValidateUiPackageId(packageId);
        if (!idResult.Succeeded())
        {
            return {
                UiResourceMountStatus::InvalidPackageId,
                idResult.diagnostic,
            };
        }

        if (root.empty())
        {
            return {
                UiResourceMountStatus::InvalidRoot,
                "UI package mount root is empty.",
            };
        }

        if (FindPackage(packageId) != packageRoots_.end())
        {
            return {
                UiResourceMountStatus::DuplicateMount,
                "UI package mount id is already registered.",
            };
        }

        root = std::filesystem::absolute(std::move(root)).lexically_normal();
        packageRoots_.push_back(
            PackageMount{std::move(packageId), std::move(root)});
        return {UiResourceMountStatus::Ok, {}};
    }

    [[nodiscard]] UiResourceResolveResult ResolveDocument(
        const UiDocumentId& id) const override
    {
        const UiResourceIdValidationResult idResult = ValidateUiDocumentId(id);
        if (!idResult.Succeeded())
        {
            return InvalidIdResult(idResult.diagnostic);
        }

        return Resolve(id.packageId, id.name, ".rml", "document");
    }

    [[nodiscard]] UiResourceResolveResult ResolveStyle(
        const UiStyleId& id) const override
    {
        const UiResourceIdValidationResult idResult = ValidateUiStyleId(id);
        if (!idResult.Succeeded())
        {
            return InvalidIdResult(idResult.diagnostic);
        }

        return Resolve(id.packageId, id.name, ".rcss", "style");
    }

    [[nodiscard]] std::size_t ContentMountCount() const noexcept override
    {
        return contentRoots_.size();
    }

    [[nodiscard]] std::size_t PackageMountCount() const noexcept override
    {
        return packageRoots_.size();
    }

private:
    struct PackageMount
    {
        std::string packageId;
        std::filesystem::path root;
    };

    [[nodiscard]] bool ContainsContentRoot(
        const std::filesystem::path& root) const
    {
        return std::find(contentRoots_.begin(), contentRoots_.end(), root) !=
               contentRoots_.end();
    }

    [[nodiscard]] std::vector<PackageMount>::const_iterator FindPackage(
        std::string_view packageId) const
    {
        return std::find_if(
            packageRoots_.begin(),
            packageRoots_.end(),
            [packageId](const PackageMount& mount)
            {
                return mount.packageId == packageId;
            });
    }

    [[nodiscard]] UiResourceResolveResult Resolve(
        const std::string& packageId,
        const std::string& name,
        const char* extension,
        const char* kind) const
    {
        if (!packageId.empty())
        {
            const auto package = FindPackage(packageId);
            if (package == packageRoots_.end())
            {
                return {
                    UiResourceResolveStatus::MountNotFound,
                    {},
                    "UI package mount was not found.",
                };
            }

            return ResolveAtRoot(package->root, name, extension, kind);
        }

        if (contentRoots_.empty())
        {
            return {
                UiResourceResolveStatus::MountNotFound,
                {},
                "No UI content mount roots are registered.",
            };
        }

        UiResourceResolveResult lastResult;
        for (const std::filesystem::path& root : contentRoots_)
        {
            lastResult = ResolveAtRoot(root, name, extension, kind);
            if (lastResult.Succeeded())
            {
                return lastResult;
            }
        }

        return lastResult;
    }

    [[nodiscard]] UiResourceResolveResult ResolveAtRoot(
        const std::filesystem::path& root,
        const std::string& name,
        const char* extension,
        const char* kind) const
    {
        const std::filesystem::path candidate = UiPath(root, name, extension);
        if (!std::filesystem::exists(candidate))
        {
            return {
                UiResourceResolveStatus::ResourceNotFound,
                candidate,
                std::string("UI ") + kind + " was not found.",
            };
        }

        return {
            UiResourceResolveStatus::Ok,
            candidate,
            {},
        };
    }

    std::vector<std::filesystem::path> contentRoots_;
    std::vector<PackageMount> packageRoots_;
};

} // namespace

std::unique_ptr<IUiResourceProvider> CreateDefaultUiResourceProvider()
{
    return std::make_unique<DefaultUiResourceProvider>();
}

} // namespace SagaEngine::UI
