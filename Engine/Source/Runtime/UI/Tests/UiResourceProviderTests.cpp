/// @file UiResourceProviderTests.cpp
/// @brief Focused tests for logical UI resource mounting and resolution.

#include <gtest/gtest.h>

#include "SagaEngine/UI/IUiResourceProvider.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace
{

namespace SagaUI = ::SagaEngine::UI;

[[nodiscard]] std::filesystem::path TestRoot(const std::string& name)
{
    return std::filesystem::temp_directory_path() /
           "SagaUiResourceProviderTests" / name;
}

void WriteTextFile(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    output << "test";
}

} // namespace

TEST(UiResourceProviderTests, ValidatesLogicalIds)
{
    EXPECT_TRUE(SagaUI::ValidateUiDocumentId(
                    SagaUI::UiDocumentId::FromContent("connect_screen"))
                    .Succeeded());
    EXPECT_TRUE(SagaUI::ValidateUiStyleId(
                    SagaUI::UiStyleId::FromPackage(
                        "base_package",
                        "menus/main"))
                    .Succeeded());

    EXPECT_EQ(
        SagaUI::ValidateUiDocumentId(SagaUI::UiDocumentId::FromContent(""))
            .code,
        SagaUI::UiResourceIdValidationCode::EmptyName);
    EXPECT_EQ(
        SagaUI::ValidateUiDocumentId(SagaUI::UiDocumentId::FromContent("../menu"))
            .code,
        SagaUI::UiResourceIdValidationCode::InvalidPathSegment);
    EXPECT_EQ(
        SagaUI::ValidateUiDocumentId(
            SagaUI::UiDocumentId::FromPackage("bad/package", "menu"))
            .code,
        SagaUI::UiResourceIdValidationCode::InvalidCharacter);
}

TEST(UiResourceProviderTests, ResolvesContentMountDocumentsAndStyles)
{
    const std::filesystem::path root = TestRoot("ContentMount");
    std::filesystem::remove_all(root);
    WriteTextFile(root / "UI" / "connect_screen.rml");
    WriteTextFile(root / "UI" / "connect_screen.rcss");

    std::unique_ptr<SagaUI::IUiResourceProvider> provider =
        SagaUI::CreateDefaultUiResourceProvider();
    ASSERT_NE(provider, nullptr);
    EXPECT_TRUE(provider->MountContentRoot(root).Succeeded());

    const SagaUI::UiResourceResolveResult document =
        provider->ResolveDocument(
            SagaUI::UiDocumentId::FromContent("connect_screen"));
    ASSERT_TRUE(document.Succeeded()) << document.diagnostic;
    EXPECT_EQ(document.path, root / "UI" / "connect_screen.rml");

    const SagaUI::UiResourceResolveResult style =
        provider->ResolveStyle(SagaUI::UiStyleId::FromContent("connect_screen"));
    ASSERT_TRUE(style.Succeeded()) << style.diagnostic;
    EXPECT_EQ(style.path, root / "UI" / "connect_screen.rcss");

    std::filesystem::remove_all(root);
}

TEST(UiResourceProviderTests, ResolvesPackageMountDocuments)
{
    const std::filesystem::path root = TestRoot("PackageMount");
    std::filesystem::remove_all(root);
    WriteTextFile(root / "UI" / "screens" / "connect_screen.rml");

    std::unique_ptr<SagaUI::IUiResourceProvider> provider =
        SagaUI::CreateDefaultUiResourceProvider();
    ASSERT_NE(provider, nullptr);
    EXPECT_TRUE(provider->MountPackageRoot("starter_pack", root).Succeeded());

    const SagaUI::UiResourceResolveResult document =
        provider->ResolveDocument(
            SagaUI::UiDocumentId::FromPackage(
                "starter_pack",
                "screens/connect_screen"));
    ASSERT_TRUE(document.Succeeded()) << document.diagnostic;
    EXPECT_EQ(document.path, root / "UI" / "screens" / "connect_screen.rml");

    std::filesystem::remove_all(root);
}

TEST(UiResourceProviderTests, ReportsMissingIdsDeterministically)
{
    const std::filesystem::path root = TestRoot("MissingId");
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "UI");

    std::unique_ptr<SagaUI::IUiResourceProvider> provider =
        SagaUI::CreateDefaultUiResourceProvider();
    ASSERT_NE(provider, nullptr);
    EXPECT_TRUE(provider->MountContentRoot(root).Succeeded());

    const SagaUI::UiResourceResolveResult missing =
        provider->ResolveDocument(SagaUI::UiDocumentId::FromContent("missing"));
    EXPECT_EQ(missing.status, SagaUI::UiResourceResolveStatus::ResourceNotFound);
    EXPECT_EQ(missing.path, root / "UI" / "missing.rml");
    EXPECT_FALSE(missing.diagnostic.empty());

    const SagaUI::UiResourceResolveResult noPackage =
        provider->ResolveDocument(
            SagaUI::UiDocumentId::FromPackage("unknown_package", "missing"));
    EXPECT_EQ(noPackage.status, SagaUI::UiResourceResolveStatus::MountNotFound);
    EXPECT_FALSE(noPackage.diagnostic.empty());

    std::filesystem::remove_all(root);
}

TEST(UiResourceProviderTests, RejectsDuplicateMounts)
{
    const std::filesystem::path root = TestRoot("DuplicateMount");
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    std::unique_ptr<SagaUI::IUiResourceProvider> provider =
        SagaUI::CreateDefaultUiResourceProvider();
    ASSERT_NE(provider, nullptr);

    EXPECT_TRUE(provider->MountContentRoot(root).Succeeded());
    EXPECT_EQ(
        provider->MountContentRoot(root).status,
        SagaUI::UiResourceMountStatus::DuplicateMount);
    EXPECT_EQ(provider->ContentMountCount(), 1u);

    EXPECT_TRUE(provider->MountPackageRoot("starter_pack", root).Succeeded());
    EXPECT_EQ(
        provider->MountPackageRoot("starter_pack", root).status,
        SagaUI::UiResourceMountStatus::DuplicateMount);
    EXPECT_EQ(provider->PackageMountCount(), 1u);

    std::filesystem::remove_all(root);
}
