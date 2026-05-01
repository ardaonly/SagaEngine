/// @file PersonaTests.cpp
/// @brief GoogleTest coverage for the UI persona framework.

#include "SagaEditor/Persona/BlockVisualStyle.h"
#include "SagaEditor/Persona/DensityProfile.h"
#include "SagaEditor/Persona/PersonaActivator.h"
#include "SagaEditor/Persona/PersonaRegistry.h"
#include "SagaEditor/Persona/UIPersona.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <string>

namespace
{

using namespace SagaEditor;

// ─── DensityProfile ───────────────────────────────────────────────────────────

TEST(DensityProfileTest, BuiltinIdsAreStableAndUnique)
{
    const DensityStep all[] = {
        DensityStep::Cosy, DensityStep::Comfortable, DensityStep::Compact,
        DensityStep::Dense, DensityStep::Ultra, DensityStep::Custom,
    };
    std::string seen[6];
    for (size_t i = 0; i < std::size(all); ++i)
    {
        seen[i] = DensityStepId(all[i]);
        EXPECT_FALSE(seen[i].empty());
        for (size_t j = 0; j < i; ++j)
        {
            EXPECT_NE(seen[i], seen[j]);
        }
        // Round-trip through the inverse.
        EXPECT_EQ(DensityStepFromId(seen[i]), all[i]);
    }
    // Unknown id falls back to Comfortable.
    EXPECT_EQ(DensityStepFromId("not.a.real.id"), DensityStep::Comfortable);
}

TEST(DensityProfileTest, FactoriesProduceMonotonicallyShrinkingMetrics)
{
    auto cosy        = MakeCosyDensity();
    auto comfortable = MakeComfortableDensity();
    auto compact     = MakeCompactDensity();
    auto dense       = MakeDenseDensity();
    auto ultra       = MakeUltraDensity();

    // Padding should shrink as we move along the spectrum.
    EXPECT_GT(cosy.basePadding,        comfortable.basePadding);
    EXPECT_GE(comfortable.basePadding, compact.basePadding);
    EXPECT_GE(compact.basePadding,     dense.basePadding);
    EXPECT_GE(dense.basePadding,       ultra.basePadding);

    // Fonts shrink the same way.
    EXPECT_GT(cosy.fontBodyPt,        comfortable.fontBodyPt);
    EXPECT_GT(comfortable.fontBodyPt, ultra.fontBodyPt);

    // Hit boxes too.
    EXPECT_GT(cosy.hitBoxMinPx, ultra.hitBoxMinPx);
}

TEST(DensityProfileTest, MakeDensityProfileSwitchesByStep)
{
    EXPECT_EQ(MakeDensityProfile(DensityStep::Cosy).step,        DensityStep::Cosy);
    EXPECT_EQ(MakeDensityProfile(DensityStep::Compact).step,     DensityStep::Compact);
    EXPECT_EQ(MakeDensityProfile(DensityStep::Ultra).step,       DensityStep::Ultra);
    auto custom = MakeDensityProfile(DensityStep::Custom);
    EXPECT_EQ(custom.step, DensityStep::Custom);
    EXPECT_EQ(custom.displayName, "Custom");
}

TEST(DensityProfileTest, EqualityIsValueBased)
{
    auto a = MakeCompactDensity();
    auto b = MakeCompactDensity();
    EXPECT_EQ(a, b);
    b.basePadding += 1.0f;
    EXPECT_NE(a, b);
}

// ─── BlockVisualStyle ─────────────────────────────────────────────────────────

TEST(BlockVisualStyleTest, EnumIdsRoundTrip)
{
    EXPECT_EQ(BlockShapeFromId(BlockShapeId(BlockShape::Stacked)), BlockShape::Stacked);
    EXPECT_EQ(BlockShapeFromId(BlockShapeId(BlockShape::PinNode)), BlockShape::PinNode);
    EXPECT_EQ(BlockShapeFromId(BlockShapeId(BlockShape::Hybrid)),  BlockShape::Hybrid);
    EXPECT_EQ(BlockShapeFromId("garbage"), BlockShape::PinNode); // default fallback

    EXPECT_EQ(ConnectorStyleFromId(ConnectorStyleId(ConnectorStyle::Notched)),
              ConnectorStyle::Notched);
    EXPECT_EQ(ConnectorStyleFromId(ConnectorStyleId(ConnectorStyle::Bezier)),
              ConnectorStyle::Bezier);

    EXPECT_EQ(SlotShapeFromId(SlotShapeId(SlotShape::Pill)),    SlotShape::Pill);
    EXPECT_EQ(SlotShapeFromId(SlotShapeId(SlotShape::Hexagon)), SlotShape::Hexagon);
}

TEST(BlockVisualStyleTest, StackedAndPinNodeDifferOnCoreFields)
{
    auto stacked = MakeStackedBlocksStyle();
    auto pin     = MakePinNodeStyle();

    EXPECT_EQ(stacked.shape,          BlockShape::Stacked);
    EXPECT_EQ(stacked.connectorStyle, ConnectorStyle::Notched);
    EXPECT_FALSE(stacked.showPinLabels);
    EXPECT_FALSE(stacked.showPinTypes);
    EXPECT_TRUE (stacked.showCategoryIcons);
    EXPECT_GT   (stacked.cornerRadius, pin.cornerRadius);
    EXPECT_GT   (stacked.minNodeWidth, pin.minNodeWidth);

    EXPECT_EQ(pin.shape,          BlockShape::PinNode);
    EXPECT_EQ(pin.connectorStyle, ConnectorStyle::Bezier);
    EXPECT_TRUE(pin.showPinLabels);
    EXPECT_TRUE(pin.showPinTypes);
}

TEST(BlockVisualStyleTest, CompactPinIsTighterThanPin)
{
    auto pin     = MakePinNodeStyle();
    auto compact = MakeCompactPinStyle();
    EXPECT_LT(compact.cornerRadius, pin.cornerRadius);
    EXPECT_LT(compact.headerHeight, pin.headerHeight);
    EXPECT_LT(compact.minNodeWidth, pin.minNodeWidth);
    EXPECT_FALSE(compact.showPinLabels);
}

TEST(BlockVisualStyleTest, EqualityIsValueBased)
{
    auto a = MakePinNodeStyle();
    auto b = MakePinNodeStyle();
    EXPECT_EQ(a, b);
    b.cornerRadius += 1.0f;
    EXPECT_NE(a, b);
}

// ─── UIPersona ────────────────────────────────────────────────────────────────

TEST(UIPersonaTest, BuiltinPersonasHaveDistinctIds)
{
    auto beginner = MakeBlockyBeginnerPersona();
    auto indie    = MakeIndieBalancedPersona();
    auto pro      = MakeProDensePersona();
    auto tech     = MakeTechnicalPersona();
    auto custom   = MakeCustomPersona();

    const std::string ids[] = { beginner.id, indie.id, pro.id, tech.id, custom.id };
    for (size_t i = 0; i < std::size(ids); ++i)
    {
        EXPECT_FALSE(ids[i].empty());
        for (size_t j = 0; j < i; ++j)
        {
            EXPECT_NE(ids[i], ids[j]);
        }
    }
}

TEST(UIPersonaTest, BeginnerHasStackedBlocksAndCosyDensity)
{
    auto p = MakeBlockyBeginnerPersona();
    EXPECT_EQ(p.tier,                  PersonaTier::Beginner);
    EXPECT_EQ(p.density.step,          DensityStep::Cosy);
    EXPECT_EQ(p.blockStyle.shape,      BlockShape::Stacked);
    EXPECT_FALSE(p.exposesProfiler);
    EXPECT_FALSE(p.exposesAssetBrowser);
    EXPECT_FALSE(p.showShortcutHints);
}

TEST(UIPersonaTest, ProDenseHasPinNodeAndCompactDensity)
{
    auto p = MakeProDensePersona();
    EXPECT_EQ(p.tier,                 PersonaTier::Pro);
    EXPECT_EQ(p.density.step,         DensityStep::Compact);
    EXPECT_EQ(p.blockStyle.shape,     BlockShape::PinNode);
    EXPECT_TRUE(p.exposesProfiler);
    EXPECT_TRUE(p.exposesAssetBrowser);
    EXPECT_TRUE(p.showShortcutHints);
    // Pro persona ships every default panel the editor knows about.
    const std::vector<std::string>& panels = p.defaultPanels;
    EXPECT_TRUE(std::find(panels.begin(), panels.end(),
                          "saga.panel.viewport") != panels.end());
    EXPECT_TRUE(std::find(panels.begin(), panels.end(),
                          "saga.panel.profiler") != panels.end());
}

TEST(UIPersonaTest, TierIdRoundTripsAndDefaultsToIndie)
{
    EXPECT_EQ(PersonaTierFromId("saga.tier.beginner"), PersonaTier::Beginner);
    EXPECT_EQ(PersonaTierFromId("saga.tier.pro"),      PersonaTier::Pro);
    EXPECT_EQ(PersonaTierFromId("not.a.tier"),         PersonaTier::Indie);
}

TEST(UIPersonaTest, DefaultPersonaIdForTierMatchesFactory)
{
    EXPECT_EQ(DefaultPersonaIdForTier(PersonaTier::Beginner),
              MakeBlockyBeginnerPersona().id);
    EXPECT_EQ(DefaultPersonaIdForTier(PersonaTier::Pro),
              MakeProDensePersona().id);
    EXPECT_EQ(DefaultPersonaIdForTier(PersonaTier::Custom),
              MakeIndieBalancedPersona().id);
}

// ─── PersonaRegistry ──────────────────────────────────────────────────────────

class PersonaRegistryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_registry.RegisterBuiltinPersonas();
    }

    PersonaRegistry m_registry;
};

TEST_F(PersonaRegistryTest, BuiltinRegistrationProducesFivePersonas)
{
    EXPECT_EQ(m_registry.Size(), 5u);
    EXPECT_TRUE(m_registry.Has("saga.persona.beginner"));
    EXPECT_TRUE(m_registry.Has("saga.persona.indie"));
    EXPECT_TRUE(m_registry.Has("saga.persona.pro"));
    EXPECT_TRUE(m_registry.Has("saga.persona.technical"));
    EXPECT_TRUE(m_registry.Has("saga.persona.custom"));
}

TEST_F(PersonaRegistryTest, GetAllSortedByTierThenId)
{
    auto all = m_registry.GetAll();
    ASSERT_EQ(all.size(), 5u);
    EXPECT_EQ(all[0].tier, PersonaTier::Beginner);
    // Tier ordering is monotonically non-decreasing.
    for (size_t i = 1; i < all.size(); ++i)
    {
        EXPECT_LE(static_cast<int>(all[i - 1].tier),
                  static_cast<int>(all[i].tier));
    }
}

TEST_F(PersonaRegistryTest, RegisterRejectsEmptyId)
{
    UIPersona p;
    p.id = ""; // empty
    EXPECT_FALSE(m_registry.Register(p));
    EXPECT_EQ(m_registry.Size(), 5u);
}

TEST_F(PersonaRegistryTest, ReregisterReplacesPreviousEntry)
{
    UIPersona replacement = MakeProDensePersona();
    replacement.displayName = "Pro Dense (Override)";
    EXPECT_TRUE(m_registry.Register(replacement));
    EXPECT_EQ(m_registry.Size(), 5u);
    const auto* found = m_registry.Find("saga.persona.pro");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->displayName, "Pro Dense (Override)");
}

TEST_F(PersonaRegistryTest, UnregisterClearsActiveWhenSameId)
{
    EXPECT_TRUE(m_registry.SetActive("saga.persona.pro"));
    EXPECT_EQ(m_registry.GetActiveId(), "saga.persona.pro");
    EXPECT_TRUE(m_registry.Unregister("saga.persona.pro"));
    EXPECT_EQ(m_registry.GetActiveId(), "");
    EXPECT_EQ(m_registry.GetActive(), nullptr);
}

TEST_F(PersonaRegistryTest, SetActiveFiresChangeCallback)
{
    std::atomic<int> callCount{0};
    std::string seenId;
    auto sub = m_registry.OnPersonaChanged(
        [&](const UIPersona& p)
        {
            ++callCount;
            seenId = p.id;
        });
    EXPECT_NE(sub, kInvalidPersonaSubscription);
    EXPECT_EQ(m_registry.SubscriberCount(), 1u);

    EXPECT_TRUE(m_registry.SetActive("saga.persona.beginner"));
    EXPECT_EQ(callCount.load(), 1);
    EXPECT_EQ(seenId, "saga.persona.beginner");

    // Switching to a non-existent persona does not fire the callback.
    EXPECT_FALSE(m_registry.SetActive("saga.persona.bogus"));
    EXPECT_EQ(callCount.load(), 1);

    EXPECT_TRUE(m_registry.RemovePersonaChangedCallback(sub));
    EXPECT_FALSE(m_registry.RemovePersonaChangedCallback(sub));
}

TEST_F(PersonaRegistryTest, EmptyCallbackRejected)
{
    auto sub = m_registry.OnPersonaChanged({});
    EXPECT_EQ(sub, kInvalidPersonaSubscription);
    EXPECT_EQ(m_registry.SubscriberCount(), 0u);
}

TEST_F(PersonaRegistryTest, ThrowingSubscriberDoesNotBreakDispatch)
{
    std::atomic<int> goodCount{0};
    m_registry.OnPersonaChanged(
        [](const UIPersona&) { throw std::runtime_error("boom"); });
    m_registry.OnPersonaChanged(
        [&goodCount](const UIPersona&) { ++goodCount; });

    EXPECT_TRUE(m_registry.SetActive("saga.persona.indie"));
    EXPECT_EQ(goodCount.load(), 1);
}

// ─── PersonaActivator ─────────────────────────────────────────────────────────

class PersonaActivatorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_registry.RegisterBuiltinPersonas();
    }

    PersonaRegistry  m_registry;
    PersonaActivator m_activator;
};

TEST_F(PersonaActivatorTest, NoSinksMeansNothingApplied)
{
    auto p      = MakeProDensePersona();
    auto result = m_activator.Apply(p);
    EXPECT_FALSE(result.AnyApplied());
    EXPECT_FALSE(result.AllApplied());
    EXPECT_TRUE (result.lastError.empty());
}

TEST_F(PersonaActivatorTest, ApplyFansOutToEverySink)
{
    std::string seenTheme;
    std::string seenLayout;
    DensityProfile seenDensity;
    BlockVisualStyle seenStyle;
    std::vector<std::string> seenToolbar;
    std::vector<std::string> seenPanels;

    m_activator.SetThemeSink(
        [&](const std::string& id) { seenTheme = id; return true; });
    m_activator.SetLayoutSink(
        [&](const std::string& id) { seenLayout = id; return true; });
    m_activator.SetDensitySink(
        [&](const DensityProfile& d) { seenDensity = d; return true; });
    m_activator.SetBlockStyleSink(
        [&](const BlockVisualStyle& s) { seenStyle = s; return true; });
    m_activator.SetToolbarSink(
        [&](const std::vector<std::string>& cmds) { seenToolbar = cmds; return true; });
    m_activator.SetPanelVisibilitySink(
        [&](const std::vector<std::string>& panels) { seenPanels = panels; return true; });

    auto p      = MakeProDensePersona();
    auto result = m_activator.Apply(p);
    EXPECT_TRUE(result.AllApplied());
    EXPECT_EQ(seenTheme,  p.themeId);
    EXPECT_EQ(seenLayout, p.workspacePresetId);
    EXPECT_EQ(seenDensity, p.density);
    EXPECT_EQ(seenStyle,   p.blockStyle);
    EXPECT_EQ(seenToolbar, p.defaultToolbarCommands);
    EXPECT_EQ(seenPanels,  p.defaultPanels);
}

TEST_F(PersonaActivatorTest, FailingSinkDoesNotAbortLaterSinks)
{
    std::atomic<int> calls{0};
    m_activator.SetThemeSink(
        [&](const std::string&) { ++calls; return false; });   // fails
    m_activator.SetLayoutSink(
        [&](const std::string&) { ++calls; return true;  });   // still runs
    m_activator.SetDensitySink(
        [&](const DensityProfile&) { ++calls; return true; }); // still runs

    auto result = m_activator.Apply(MakeIndieBalancedPersona());
    EXPECT_EQ(calls.load(), 3);
    EXPECT_FALSE(result.themeApplied);
    EXPECT_TRUE (result.layoutApplied);
    EXPECT_TRUE (result.densityApplied);
    EXPECT_FALSE(result.AllApplied());
    EXPECT_TRUE (result.AnyApplied());
    EXPECT_FALSE(result.lastError.empty());
}

TEST_F(PersonaActivatorTest, ApplyActivePullsFromRegistry)
{
    std::string seenTheme;
    m_activator.SetThemeSink(
        [&](const std::string& id) { seenTheme = id; return true; });

    EXPECT_TRUE(m_registry.SetActive("saga.persona.beginner"));
    auto result = m_activator.ApplyActive(m_registry);
    EXPECT_TRUE(result.themeApplied);
    EXPECT_EQ(seenTheme, MakeBlockyBeginnerPersona().themeId);
}

TEST_F(PersonaActivatorTest, ApplyActiveWithoutActiveReportsError)
{
    PersonaRegistry empty;
    auto result = m_activator.ApplyActive(empty);
    EXPECT_FALSE(result.AnyApplied());
    EXPECT_FALSE(result.lastError.empty());
}

TEST_F(PersonaActivatorTest, ClearSinksDisablesEveryFanout)
{
    std::atomic<int> calls{0};
    m_activator.SetThemeSink([&](const std::string&) { ++calls; return true; });
    m_activator.SetLayoutSink([&](const std::string&) { ++calls; return true; });
    m_activator.ClearSinks();

    auto result = m_activator.Apply(MakeIndieBalancedPersona());
    EXPECT_EQ(calls.load(), 0);
    EXPECT_FALSE(result.AnyApplied());
}

} // namespace
