/// @file CompileStateTests.cpp
/// @brief Tests for CompileState severity ordering and merge logic.

#include "SDE/Validation/CompileState.h"

#include <gtest/gtest.h>

using namespace SDE;

TEST(CompileStateTest, MergeReturnsSeverityOrder)
{
    EXPECT_EQ(Merge(CompileState::Clean, CompileState::WithWarnings),  CompileState::WithWarnings);
    EXPECT_EQ(Merge(CompileState::WithWarnings, CompileState::Clean),  CompileState::WithWarnings);
    EXPECT_EQ(Merge(CompileState::Clean, CompileState::ValidationFailed), CompileState::ValidationFailed);
    EXPECT_EQ(Merge(CompileState::Aborted, CompileState::UnresolvedRefs), CompileState::Aborted);
}

TEST(CompileStateTest, MergeIsCommutative)
{
    auto a = CompileState::UnresolvedRefs;
    auto b = CompileState::ValidationFailed;
    EXPECT_EQ(Merge(a, b), Merge(b, a));
}

TEST(CompileStateTest, IsUsableReturnsTrueForCleanAndWarnings)
{
    EXPECT_TRUE(IsUsable(CompileState::Clean));
    EXPECT_TRUE(IsUsable(CompileState::WithWarnings));
}

TEST(CompileStateTest, IsUsableReturnsFalseForFailedStates)
{
    EXPECT_FALSE(IsUsable(CompileState::UnresolvedRefs));
    EXPECT_FALSE(IsUsable(CompileState::ValidationFailed));
    EXPECT_FALSE(IsUsable(CompileState::Aborted));
    EXPECT_FALSE(IsUsable(CompileState::IOError));
}

TEST(CompileStateTest, StateNameReturnsNonEmptyString)
{
    EXPECT_EQ(StateName(CompileState::Clean),            "Clean");
    EXPECT_EQ(StateName(CompileState::WithWarnings),     "WithWarnings");
    EXPECT_EQ(StateName(CompileState::UnresolvedRefs),   "UnresolvedRefs");
    EXPECT_EQ(StateName(CompileState::ValidationFailed), "ValidationFailed");
    EXPECT_EQ(StateName(CompileState::Aborted),          "Aborted");
    EXPECT_EQ(StateName(CompileState::IOError),          "IOError");
}
