// SPDX-License-Identifier: MPL-2.0
#include "SagaEditor/Authoring/AuthoringMode.h"
#include "SagaEditor/Settings/MemoryEditorSettingsStore.h"
#include <gtest/gtest.h>

namespace SagaEditor
{
TEST(AuthoringModeTests, ExposesExactlyThreeStableBuiltinsAndDefaultsToLevel1)
{
    MemoryEditorSettingsStore settings;
    AuthoringModeService service;
    ASSERT_TRUE(service.Init(settings));
    ASSERT_EQ(service.Modes().size(), 3u);
    EXPECT_EQ(service.Modes()[0].id, "level1");
    EXPECT_EQ(service.Modes()[1].id, "level2");
    EXPECT_EQ(service.Modes()[2].id, "level3");
    EXPECT_EQ(service.Active().id, "level1");
    EXPECT_EQ(settings.GetString(AuthoringModeService::SettingKey, {}), "level1");
}

TEST(AuthoringModeTests, ExplicitValidWinsAndExplicitUnknownFails)
{
    MemoryEditorSettingsStore settings;
    settings.SetString(AuthoringModeService::SettingKey, "level2");
    AuthoringModeService valid;
    ASSERT_TRUE(valid.Init(settings, "level3"));
    EXPECT_EQ(valid.Active().id, "level3");
    AuthoringModeService invalid;
    EXPECT_FALSE(invalid.Init(settings, "unknown"));
}

TEST(AuthoringModeTests, MigratesProfileBeforePersonaIdempotently)
{
    MemoryEditorSettingsStore settings;
    settings.SetString("profile.activeId", "saga.profile.creator");
    settings.SetString("persona.activeId", "saga.persona.technical");
    MigrateLegacyEditorPreferences(settings);
    EXPECT_EQ(settings.GetString(AuthoringModeService::SettingKey, {}), "level1");
    EXPECT_EQ(settings.GetString(
        AuthoringModeService::MigrationVersionKey, {}), "2");
    EXPECT_EQ(settings.GetString(
        AuthoringModeService::ThemeSettingKey, {}), "Midnight");
    settings.SetString("profile.activeId", "saga.profile.advanced_pipeline");
    MigrateLegacyEditorPreferences(settings);
    EXPECT_EQ(settings.GetString(AuthoringModeService::SettingKey, {}), "level1");
}

TEST(AuthoringModeTests, MapsEveryLegacyBuiltinDeterministically)
{
    const std::pair<const char*, const char*> cases[] = {
        {"saga.profile.basic", "level1"},
        {"saga.persona.beginner", "level1"},
        {"saga.profile.node_editor", "level2"},
        {"saga.profile.standard_pipeline", "level2"},
        {"saga.profile.custom", "level2"},
        {"saga.persona.indie", "level2"},
        {"saga.persona.custom", "level2"},
        {"saga.profile.advanced_pipeline", "level3"},
        {"saga.persona.pro", "level3"},
        {"saga.persona.technical", "level3"},
    };
    for (const auto& [legacy, expected] : cases)
        EXPECT_EQ(AuthoringModeForLegacyPreference(legacy), expected) << legacy;
}

TEST(AuthoringModeTests, MissingAndPartialLegacySettingsCompleteMigrationOnce)
{
    MemoryEditorSettingsStore settings;
    settings.SetString("persona.activeId", "saga.persona.technical");
    MigrateLegacyEditorPreferences(settings);
    EXPECT_EQ(settings.GetString(AuthoringModeService::SettingKey, {}), "level3");
    settings.SetString("profile.activeId", "saga.profile.basic");
    MigrateLegacyEditorPreferences(settings);
    EXPECT_EQ(settings.GetString(AuthoringModeService::SettingKey, {}), "level3");
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
        settings.SetString(AuthoringModeService::SettingKey, "level2");
        settings.SetString("persona.activeId", persona);

        MigrateLegacyEditorPreferences(settings);

        EXPECT_EQ(settings.GetString(
            AuthoringModeService::ThemeSettingKey, {}), expected) << persona;
        EXPECT_EQ(settings.GetString(
            AuthoringModeService::SettingKey, {}), "level2") << persona;
        EXPECT_EQ(settings.GetString(
            AuthoringModeService::MigrationVersionKey, {}), "2") << persona;
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
        AuthoringModeService::MigrationVersionKey, {}), "2");
}
} // namespace SagaEditor
