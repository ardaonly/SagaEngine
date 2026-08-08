// SPDX-License-Identifier: MPL-2.0
#include "SagaEditor/Authoring/AuthoringMode.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"
#include <gtest/gtest.h>

namespace SagaEditor
{
TEST(AuthoringModeTests, ExposesExactlyThreeStableOfficialWorkModes)
{
    MemoryEditorSettingsStore settings;
    AuthoringModeService service;
    ASSERT_TRUE(service.Init(settings));
    ASSERT_EQ(service.Modes().size(), 3u);
    EXPECT_EQ(service.Modes()[0].id, "quick-production");
    EXPECT_EQ(service.Modes()[1].id, "balanced");
    EXPECT_EQ(service.Modes()[2].id, "full-control");
    EXPECT_EQ(service.Active().id, "quick-production");
    EXPECT_EQ(settings.GetString(AuthoringModeService::SettingKey, {}),
        "quick-production");
}

TEST(AuthoringModeTests, ExplicitValidWinsAndExplicitUnknownFails)
{
    MemoryEditorSettingsStore settings;
    settings.SetString(AuthoringModeService::SettingKey, "balanced");
    AuthoringModeService valid;
    ASSERT_TRUE(valid.Init(settings, "full-control"));
    EXPECT_EQ(valid.Active().id, "full-control");
    AuthoringModeService invalid;
    EXPECT_FALSE(invalid.Init(settings, "unknown"));
}

TEST(AuthoringModeTests, MigratesProfileBeforePersonaIdempotently)
{
    MemoryEditorSettingsStore settings;
    settings.SetString("profile.activeId", "saga.profile.creator");
    settings.SetString("persona.activeId", "saga.persona.technical");
    MigrateLegacyEditorPreferences(settings);
    EXPECT_EQ(settings.GetString(AuthoringModeService::SettingKey, {}),
        "quick-production");
    EXPECT_EQ(settings.GetString(
        AuthoringModeService::MigrationVersionKey, {}), "3");
    EXPECT_EQ(settings.GetString(
        AuthoringModeService::ThemeSettingKey, {}), "Midnight");
    settings.SetString("profile.activeId", "saga.profile.advanced_pipeline");
    MigrateLegacyEditorPreferences(settings);
    EXPECT_EQ(settings.GetString(AuthoringModeService::SettingKey, {}),
        "quick-production");
}

TEST(AuthoringModeTests, MapsEveryLegacyBuiltinDeterministically)
{
    const std::pair<const char*, const char*> cases[] = {
        {"saga.profile.basic", "quick-production"},
        {"saga.persona.beginner", "quick-production"},
        {"saga.profile.node_editor", "balanced"},
        {"saga.profile.standard_pipeline", "balanced"},
        {"saga.profile.custom", "balanced"},
        {"saga.persona.indie", "balanced"},
        {"saga.persona.custom", "balanced"},
        {"saga.profile.advanced_pipeline", "full-control"},
        {"saga.persona.pro", "full-control"},
        {"saga.persona.technical", "full-control"},
    };
    for (const auto& [legacy, expected] : cases)
        EXPECT_EQ(AuthoringModeForLegacyPreference(legacy), expected) << legacy;
}

TEST(AuthoringModeTests, MissingAndPartialLegacySettingsCompleteMigrationOnce)
{
    MemoryEditorSettingsStore settings;
    settings.SetString("persona.activeId", "saga.persona.technical");
    MigrateLegacyEditorPreferences(settings);
    EXPECT_EQ(settings.GetString(AuthoringModeService::SettingKey, {}),
        "full-control");
    settings.SetString("profile.activeId", "saga.profile.basic");
    MigrateLegacyEditorPreferences(settings);
    EXPECT_EQ(settings.GetString(AuthoringModeService::SettingKey, {}),
        "full-control");
}

TEST(AuthoringModeTests, MigratesEveryLegacyBuiltinPersonaThemeFromVersion1)
{
    const std::pair<const char*, const char*> cases[] = {
        {"saga.persona.beginner", "Light"},
        {"saga.persona.indie", "Dark"},
        {"saga.persona.custom", "Dark"},
        {"saga.persona.pro", "Midnight"},
        {"saga.persona.technical", "Midnight"},
    };
    for (const auto& [persona, expected] : cases)
    {
        MemoryEditorSettingsStore settings;
        settings.SetString(AuthoringModeService::MigrationVersionKey, "1");
        settings.SetString(AuthoringModeService::SettingKey, "balanced");
        settings.SetString("persona.activeId", persona);

        MigrateLegacyEditorPreferences(settings);

        EXPECT_EQ(settings.GetString(
            AuthoringModeService::ThemeSettingKey, {}), expected) << persona;
        EXPECT_EQ(settings.GetString(
            AuthoringModeService::SettingKey, {}), "balanced") << persona;
        EXPECT_EQ(settings.GetString(
            AuthoringModeService::MigrationVersionKey, {}), "3") << persona;
    }
}

TEST(AuthoringModeTests, IndependentThemeWinsAndMigrationIsIdempotent)
{
    MemoryEditorSettingsStore settings;
    settings.SetString(AuthoringModeService::ThemeSettingKey, "Nord");
    settings.SetString("persona.activeId", "saga.persona.beginner");

    MigrateLegacyEditorPreferences(settings);
    EXPECT_EQ(settings.GetString(
        AuthoringModeService::ThemeSettingKey, {}), "Nord");

    settings.SetString("persona.activeId", "saga.persona.pro");
    MigrateLegacyEditorPreferences(settings);
    EXPECT_EQ(settings.GetString(
        AuthoringModeService::ThemeSettingKey, {}), "Nord");
}

TEST(AuthoringModeTests, UnknownPersonaDoesNotCreateThemePreference)
{
    MemoryEditorSettingsStore settings;
    settings.SetString("persona.activeId", "studio.persona.custom");

    MigrateLegacyEditorPreferences(settings);

    EXPECT_FALSE(settings.Has(AuthoringModeService::ThemeSettingKey));
    EXPECT_EQ(settings.GetString(
        AuthoringModeService::MigrationVersionKey, {}), "3");
}

TEST(AuthoringModeTests, LegacyModeAliasesResolveImmediatelyToCanonicalIds)
{
    MemoryEditorSettingsStore settings;
    EditorWorkModeService service;
    ASSERT_TRUE(service.Init(settings, "level3"));
    EXPECT_EQ(service.Active().id, "full-control");
    ASSERT_TRUE(service.SetActive("level1"));
    EXPECT_EQ(service.Active().id, "quick-production");
    EXPECT_EQ(settings.GetString(EditorWorkModeService::SettingKey, {}),
        "quick-production");
}
} // namespace SagaEditor
